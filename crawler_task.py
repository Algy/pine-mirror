# -*- coding: utf-8 -*-
import time
import os
import errno
import MySQLdb

from pprint import pprint
from multiprocessing import Lock
from datetime import timedelta
from crawler import retrieve_recent_changes, retrieve_history, \
                    retrieve_raw_document, \
                    SpoofingUrlopenException
from docdata import RecentChange, DocumentOperation, recent_change_difference
from datalayer import choose_datalayer
from celery import Celery
from crawler_config import DB_NAME, DB_USER, DB_PASSWORD, \
                           CRAWLER_RC_CRON_SECONDS, \
                           CRAWLER_RC_SECONDS, HISTORY_WINDOW_SIZE 
from traffic import SimpleTrafficClient


class Serializer(object):
    def connect(self, host="localhost", port=5123):
        if hasattr(self, 'cli'):
            self.cli.close()
        self.cli = SimpleTrafficClient()
        self.cli.connect(host, port)
        return self

    def __call__(self, fn, args=(), kwds={}):
        self.cli.wait()
        return fn(*args, **kwds)
    
    def close(self):
        self.cli.close()



app = Celery("crawler_task")
app.conf.CELERY_ROUTES = {
    'crawler_task.keep_track_of_history': {'queue': 'history-crawler'}
}

app.conf.CELERYBEAT_SCHEDULE = {
    'periodic-rc-crawling': {
        'task': 'crawler_task.keep_track_of_recent_document',
        'schedule': timedelta(seconds=CRAWLER_RC_CRON_SECONDS),
        'args': ()
    }
}

def _open_db():
    return MySQLdb.connect(user=DB_USER, db=DB_NAME, passwd=DB_PASSWORD or '', autocommit=False)


def tickle_history_worker(db, document, most_recent_revision_id):
    db.cursor().execute('''
        INSERT CrawlerHistoryContext 
            (document, most_recent_revision_id)
        VALUES
            (%s, %s)
        ON DUPLICATE KEY UPDATE
            most_recent_revision_id = VALUES(most_recent_revision_id)
        ''',
        (document, most_recent_revision_id))
    db.commit()
    keep_track_of_history.apply_async(args=(document,), queue='history-crawler')


def is_sane_404(exc):
    return exc.code == 404 and "찾을 수 없습니다" in exc.content


def trace_movement(old_name, _serializer):
    # FIXME: a kind of half measure
    for record in _serializer(retrieve_recent_changes, kwds={"logtype": "move"}):
        if (record.operation.op == DocumentOperation.MOVE and 
            record.operation.original_document == old_name):
            return record.operation.current_document
    raise KeyError(old_name)


def _ensure_table(db): 
    # id is always 0
    db.cursor().execute('''
        CREATE TABLE IF NOT EXISTS
            CrawlerRecentChangeContext (
                id INTEGER PRIMARY KEY, 
                last_recent_change_id INTEGER NOT NULL,
                FOREIGN KEY (last_recent_change_id) REFERENCES
                    RecentChange (id)
                    ON DELETE CASCADE,
                UNIQUE INDEX idx (last_recent_change_id)
            )
    ''')


NAMU_CRAWLER_DIR_LOCK = "/tmp/namu-crawler.lock"

rc_lock = Lock()
entrance_box = [False]
@app.task
def keep_track_of_recent_document():
    _serializer = Serializer().connect()
    if not _serializer.cli.try_acquire_lock():
        _serializer.close()
        return
    try:
        db = _open_db()
        delta_records = []
        try:
            _ensure_table(db)
            rc_cursor = db.cursor()

            rc_cursor.execute('''
                SELECT document, revision_id, updated_time, ip, op, author_name, comment, original_revision_id, original_document 
                FROM RecentChange
                WHERE id <=
                    (SELECT last_recent_change_id 
                     FROM CrawlerRecentChangeContext 
                     WHERE id = 0
                     LIMIT 1)
                ORDER BY id DESC
                LIMIT 100
                ''')
     
            old_records = [] # DESC
            for (document, revision_id, updated_time, 
                 ip, op, author_name, comment, 
                 original_revision_id, original_document) in rc_cursor.fetchall():

                if op == DocumentOperation.MOVE:
                    current_document = document
                else:
                    current_document = None
                record = RecentChange(document, 
                                      updated_time, 
                                      DocumentOperation(op,
                                                        original_revision_id,
                                                        original_document,
                                                        current_document), 
                                      revision_id, 
                                      ip, 
                                      author_name, 
                                      comment)
                old_records.append(record)

            new_records = _serializer(retrieve_recent_changes)
            # DESC
            delta_records = recent_change_difference(old_records, new_records)
            wiki_data = choose_datalayer("WikiDataLayer", db)
            for record in reversed(delta_records):
                print "[Got] %s" %(repr(record))
                wiki_data.add_recent_change(record) # it sets @recent_change_id
                rc_cursor.execute('''
                    REPLACE CrawlerRecentChangeContext(id, last_recent_change_id)
                    VALUES (0, @recent_change_id)
                ''')
                tickle_history_worker(db, record.document, record.revision_id)
        except:
            db.rollback()
            raise
        finally:
            db.close()
    finally:
        _serializer.cli.release_lock()
        _serializer.close()


@app.task 
def keep_track_of_history(document):
    '''
    There are several exceptions when processing history crawling.
    1. When document being processed is moved
        In this case, crawling with old name must yield '404 Not Found' exception. 
        We can simply stop and delay the current job and then retrieve recent changes page in "move" section. Then, put them into DocumentLog.

    2. When the page being requsted reject its response due to too frequent requests and asks for re-captcha.
        TODO
    '''
    _serializer = Serializer().connect()
    try:
        db = _open_db()
        def document_may_moved(document):
            try:
                new_document = trace_movement(document, _serializer)
                db.cursor().execute('''
                    DELETE FROM CrawlerHistoryContext WHERE document = %s
                    ''',
                    (new_document, ))
                db.cursor().execute('''
                    UPDATE CrawlerHistoryContext 
                    SET document = %s
                    WHERE document = %s
                    ''',
                    (new_document, document))
                keep_track_of_history.delay(new_document)
            except KeyError:
                print "[ERROR] tries to find the new name of document '%s', but failed. quit."%(document)

        try:
            _ensure_table(db)
            wiki_data = choose_datalayer("WikiDataLayer", db)
            cursor = db.cursor()
            cursor.execute('''
                SELECT MAX(revision_id)
                FROM Archive
                JOIN DocumentLog
                ON DocumentLog.id = Archive.document_log_id
                WHERE 
                    name = %s
                ''',
                (document, ))
            last_revision_id = cursor.fetchone()[0] or -1
            try:
                history_records = _serializer(
                    retrieve_history,
                    args=(document, ))
                most_recent_revision_id = history_records[0].revision_id
                while last_revision_id < most_recent_revision_id:
                    history_records = _serializer(
                        retrieve_history,
                        args=(document, ),
                        kwds={"max_revision_id": last_revision_id + HISTORY_WINDOW_SIZE})
                        # DESC order
                    for record in reversed(history_records):
                        if record.revision_id <= last_revision_id:
                            continue
                        print "process '%s' r%d"%(document, record.revision_id)
                        source = _serializer(retrieve_raw_document, args=(record.document, record.revision_id))
                        last_revision_id = record.revision_id
                        is_recent = last_revision_id == most_recent_revision_id

                        wiki_data.incremental_add(record, source, is_recent)
                        db.commit() # a transaction per history
            except SpoofingUrlopenException as exc:
                if is_sane_404(exc):
                    document_may_moved(document)
                    return
                else:
                    raise
        except:
            db.rollback()
            raise
        finally:
            db.close()
    finally:
        _serializer.close()
