import time

from multiprocessing import Lock
from docdata import DocumentOperation
from crawler import retrieve_recent_changes

_WIKI_DATA_TABLES = [
    '''
    CREATE TABLE IF NOT EXISTS 
        DocumentLog (id INTEGER AUTO_INCREMENT PRIMARY KEY,
                     name VARCHAR(100) NOT NULL,
                     UNIQUE INDEX (name));
    ''',
    '''
    CREATE TABLE IF NOT EXISTS 
        Archive (id INTEGER AUTO_INCREMENT PRIMARY KEY,
                 document_log_id INTEGER NOT NULL, 
                 updated_time DATETIME NOT NULL,
                 collected_time DATETIME,
                 revision_id INTEGER NOT NULL, 
                 source MEDIUMTEXT NOT NULL,
                 ip VARCHAR(15),
                 op VARCHAR(11) NOT NULL, 
                 author_name VARCHAR(30),
                 comment TINYTEXT NOT NULL,
                 original_revision_id INTEGER,
                 original_document VARCHAR(100),
                 FOREIGN KEY (document_log_id) REFERENCES
                    DocumentLog (id)
                    ON DELETE CASCADE,
                 UNIQUE INDEX history (document_log_id, revision_id),
                 INDEX anon_conributor (ip),
                 INDEX named_contributor (author_name)
        );
    ''',
    '''
    CREATE TABLE IF NOT EXISTS
        MoveHistory (document_log_id INTEGER NOT NULL,
                     related_archive_id INTEGER NOT NULL, 
                     old_name VARCHAR(100) NOT NULL, 
                     moved_time DATETIME NOT NULL,
                     FOREIGN KEY (document_log_id) REFERENCES
                        DocumentLog (id),
                     FOREIGN KEY (related_archive_id) REFERENCES
                        Archive (id),
                     INDEX idx_docid (document_log_id)
        );
    ''',
    '''
    CREATE TABLE IF NOT EXISTS
        LinkAssoc (src_name VARCHAR(100) NOT NULL, 
                   dest_name VARCHAR(100) NOT NULL, 
                   link_type VARCHAR(20),
                   INDEX by_src (src_name, dest_name),
                   INDEX by_dest (dest_name, src_name),
                   CONSTRAINT unique_triple UNIQUE (src_name, dest_name, link_type)
        );
    ''',
    '''
    CREATE TABLE IF NOT EXISTS
        RecentDocument (name VARCHAR(100) PRIMARY KEY,
                        deleted BOOLEAN NOT NULL DEFAULT FALSE,
                        updated_time DATETIME NOT NULL,
                        collected_time DATETIME,
                        source MEDIUMTEXT NOT NULL,
                        revision_id INTEGER NOT NULL
        );
    ''',
    '''
    CREATE TABLE IF NOT EXISTS
        RecentChange (id INTEGER AUTO_INCREMENT PRIMARY KEY, 
                      document VARCHAR(100) NOT NULL,
                      revision_id INTEGER NOT NULL,
                      updated_time DATETIME NOT NULL,
                      ip VARCHAR(15),
                      op VARCHAR(11) NOT NULL,
                      author_name VARCHAR(30),
                      comment TINYTEXT NOT NULL,
                      original_revision_id INTEGER,
                      original_document VARCHAR(100),
                      INDEX idx_doc_rev (document, revision_id)
        );
    ''',
    '''
    CREATE TABLE IF NOT EXISTS
        ArchiveCategory (id INTEGER AUTO_INCREMENT PRIMARY KEY,
                         category VARCHAR(100) NOT NULL,
                         category_revision_id INTEGER NOT NULL,
                         document VARCHAR(100) NOT NULL,
                         INDEX idx_categorizer (category, category_revision_id, document),
                         CONSTRAINT unique_triple UNIQUE (category, category_revision_id, document)
        );
    ''',
]


def extract_links(source):
    '''
    string -> (source, link_type) list
    '''
    return []



class RedisLayer(object):
    def __init__(self, redis):
        self.redis = redis

    def evict_recent_document_cache(self, document):
        # TODO
        self.redis.del_("")


class WikiDataLayer(object):
    def __init__(self, db):
        self.db = db

    def create_tables(self):
        cursor = self.db.cursor()
        for table_schema in _WIKI_DATA_TABLES:
            cursor.execute(table_schema)

    def add_recent_change(self, record):
        cursor = self.db.cursor()
        cursor.execute('''
            INSERT INTO RecentChange
                (document, updated_time, revision_id, 
                 ip, op, author_name, 
                 comment, original_revision_id, original_document)
            VALUES
                (%s, %s, %s, 
                 %s, %s, %s,
                 %s, %s, %s)
            ''',
            (record.document, record.date, record.revision_id,
             record.ip, record.operation.op, record.author_name,
             record.comment, record.operation.revision_id, record.operation.original_document))
        cursor.execute('SET @recent_change_id = LAST_INSERT_ID()')

    def incremental_add(self, record, source, is_recent):
        cursor = self.db.cursor()

        is_move_operation = record.operation.op == DocumentOperation.MOVE
        if is_move_operation:
            original_document = record.operation.original_document
            cursor.execute('''
                INSERT INTO DocumentLog (name)
                VALUES (%s)
                ON DUPLICATE KEY UPDATE
                    id = LAST_INSERT_ID(id)
                ''', 
                (original_document, )
            )
            cursor.execute('SET @document_log_id = LAST_INSERT_ID()')
            cursor.execute('''
                REPLACE DocumentLog (id, name) 
                VALUES (@document_log_id, %s)
                ''', 
                (record.document, ));
        else:
            cursor.execute('''
                INSERT INTO DocumentLog (name)
                VALUES (%s)
                ON DUPLICATE KEY UPDATE
                    id = LAST_INSERT_ID(id)
                ''', 
                (record.document, )
            )
            cursor.execute('SET @document_log_id = LAST_INSERT_ID()')

        cursor.execute('''
            INSERT INTO Archive 
                (document_log_id, updated_time, collected_time, 
                 revision_id, source, ip, 
                 op, author_name, comment,
                 original_revision_id, original_document)
            VALUES
                (@document_log_id, %s, CURRENT_TIMESTAMP(),
                 %s, %s, %s,
                 %s, %s, %s,
                 %s, %s)
            ''', 
            (record.date,
             record.revision_id, source, record.ip,
             record.operation.op, record.author_name, record.comment,
             record.operation.revision_id, record.operation.original_document))
        cursor.execute('SET @archive_id = LAST_INSERT_ID()')

        if is_recent:
            just_deleted = record.operation.op == DocumentOperation.DELETE
            cursor.execute('''
                REPLACE RecentDocument 
                    (name, deleted, updated_time, 
                     collected_time, source, revision_id)
                VALUES 
                    (%s, %s, %s,
                     CURRENT_TIMESTAMP(), %s, %s)''', 
                (record.document, just_deleted, record.date,
                                  source, record.revision_id))


        if is_move_operation:
            original_document = record.operation.original_document
            cursor.execute('''
                INSERT INTO MoveHistory 
                    (document_log_id, related_archive_id,
                     old_name, moved_time)
                VALUES
                    (@document_log_id, @archive_id, %s, CURRENT_TIMESTAMP())
                ''',
                (original_document, ))


def choose_datalayer(name, db):
    if name == 'WikiDataLayer':
        return WikiDataLayer(db)
    else:
        raise KeyError("No such datalayer as '%s'"%name)


if __name__ == '__main__':
    import MySQLdb
    import datetime

    conn = MySQLdb.connect(user='root', autocommit=False)
    try:
        conn.cursor().execute('''DROP DATABASE IF EXISTS test ''')
        conn.cursor().execute('''CREATE DATABASE test; use test''')
        
        data_layer = choose_datalayer("WikiDataLayer", conn)
        data_layer.create_tables()

        '''
        for record in reversed(retrieve_recent_changes()):
            data_layer.incremental_add(record, "Do you know kimchi?", True)
        conn.commit()
        '''
    finally:
        conn.close()

