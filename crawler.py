#!/usr/bin/env python
# -*= coding: utf-8 -*-

import gzip
import time

from datetime import datetime
from StringIO import StringIO
from urllib2 import Request, urlopen, quote
from pprint import pprint


try:
    import HTMLParser
    _html_parser = HTMLParser.HTMLParser()
    def unescape_html_entity(s):
        if isinstance(s, unicode):
            return _html_parser.unescape(s)
        else:
            return _html_parser.unescape(s.decode("UTF-8")).encode("UTF-8")

except ImportError:
    # Try to use Python3's module, 'html'
    try:
        import html
        unescape_html_entity = html.unescape
    except ImportError:
        raise ImportError("cannot import module named neither HTMLParser nor html, which is required to unescape HTML entity")





class FormatMayHaveChangedException(Exception):
    pass


def spoofing_urlopen(url):
    headers = {
        "Accept": "text/html, application/xhtml+xml",
        "Accept-Language": "ko-KR",
        "Accept-Charset": "UTF-8",
        "User-Agent": "Mozilla/5.0 (X11; U; Linux i686) Gecko/20071127 Firefox/2.0.0.11",
        "Accept-Encoding": "gzip"
    }
    req = Request(url, headers=headers)
    resp = urlopen(req)
    if resp.headers.get("Content-Encoding", None) == "gzip":
        compressed_stream = StringIO(resp.read())
        return gzip.GzipFile(fileobj=compressed_stream).read()
    else:
        return resp.read()

def split_li(s, st_tag="<li>", ed_tag="</li>"):
    try:
        result = [particle.split(ed_tag, 1)[0] 
                  for particle in filter(bool, [x.strip() for x in s.split(st_tag)])]
        return result
    except IndexError as err:
        raise FormatMayHaveChangedException(err)

class DocumentOperation(object):
    NEW = 'new'
    MODIFY = 'modify'
    DELETE = 'delete'
    REVERT = 'revert'
    MOVE = 'move'
    OPERATIONS = [NEW, 
                  MODIFY, 
                  DELETE, 
                  REVERT, 
                  MOVE]

    def __init__(self, op, revision_id=None, original_document=None, current_document=None):
        if op not in self.OPERATIONS:
            raise ValueError("%s is not proper operation."%op)
        self.op = op
        self.revision_id = revision_id
        self.original_document = original_document
        self.current_document = current_document
    
    def rc_equal(self, another):
        return (self.op == another.op and
                self.revision_id == another.revision_id and
                self.original_document == another.original_document and
                self.current_document == another.current_document)

    def __repr__(self):
        if self.op == self.REVERT:
            return "[%s to r%d]"%(self.op, self.revision_id)
        elif self.op == self.MOVE:
            return "[%s from '%s' to '%s']"%(self.op, self.original_document, self.current_document)
        else:
            return "[%s]"%self.op


    @classmethod
    def new(cls):
        return cls(cls.NEW)

    @classmethod
    def modify(cls):
        return cls(cls.MODIFY)

    @classmethod
    def delete(cls):
        return cls(cls.DELETE)

    @classmethod
    def revert(cls, revision_id):
        assert (isinstance(revision_id, (int, long,)))
        return cls(cls.REVERT, revision_id=revision_id)

    @classmethod
    def move(cls, original_document, current_document):
        return cls(cls.MOVE, original_document=original_document, current_document=current_document)


def _interpret_i(i, ro_exists=True):
    if isinstance(i, unicode):
        i = i.encode("UTF-8")
    i = i.strip()
    if i == '(\xec\x83\x88 \xeb\xac\xb8\xec\x84\x9c)': # sae moonseo
        return DocumentOperation.new()
    elif i == '(\xec\x82\xad\xec\xa0\x9c)': # sakje
        return DocumentOperation.delete()
    elif i.startswith('(r') and i.endswith('\xec\x9c\xbc\xeb\xa1\x9c \xeb\x90\x98\xeb\x8f\x8c\xeb\xa6\xbc)'): # ro dwidolim
        try:
            revision_id = int(i[2 : i.find("\xec\x9c\xbc\xeb\xa1\x9c")])
        except ValueError:
            raise FormatMayHaveChangedException
        return DocumentOperation.revert(revision_id)
    elif i.startswith('(') and i.endswith('\xeb\xac\xb8\xec\x84\x9c \xec\x9d\xb4\xeb\x8f\x99)'): # moonseo e-dong
        if ro_exists:
            s = i[1:-len('\xeb\xa1\x9c \xeb\xac\xb8\xec\x84\x9c \xec\x9d\xb4\xeb\x8f\x99)')]
            # XXX: what if a document is moved from 'ehseo ehseo esheo' (in Korean) to 'ehseo ehseo' (in Korean)
            # Unfortunately, there's no way to distinguish 'ehseo' in name of document with korean particle 'ehseo' of history log sentences.
            splited_chunks = s.rsplit('\xec\x97\x90\xec\x84\x9c ', 1) # ehseo<space>
            if len(splited_chunks) < 2:
                raise FormatMayHaveChangedException
            original_document = splited_chunks[0]
            current_document = splited_chunks[1]
            return DocumentOperation.move(original_document, current_document)
        else:
            s = i[1:-len('\xec\x97\x90\xec\x84\x9c \xeb\xac\xb8\xec\x84\x9c \xec\x9d\xb4\xeb\x8f\x99)')]
            return DocumentOperation.move(s, None)
    else:
        raise FormatMayHaveChangedException(i)


class HistoryRecord(object):
    def __init__(self, document, date, operation, revision_id, ip=None, author_name=None, comment=""):
        assert (isinstance(revision_id, (int, long,)))
        self.document = document
        self.date = date
        self.revision_id = revision_id
        self.operation = operation
        self.ip = ip
        self.author_name = author_name
        self.comment = comment

    def __repr__(self):
        return "%s%s(%s) r%d %s: \"%s\"" % (self.operation, self.document, datetime.strftime(self.date, "%Y-%m-%d %H:%M:%S"), self.revision_id, self.ip or self.author_name, self.comment)


class RecentChange(object):
    def __init__(self, document, date, operation, revision_id, ip=None, author_name=None, comment=""):
        assert (isinstance(revision_id, (int, long,)))
        self.document = document
        self.date = date
        self.operation = operation
        self.revision_id = revision_id
        self.ip = ip
        self.author_name = author_name
        self.comment = comment

    def rc_equal(self, another):
        return (self.document == another.document and
                self.date == another.date and
                self.operation.rc_equal(another.operation) and
                self.revision_id == another.revision_id and 
                self.ip == another.ip and
                self.author_name == another.author_name and
                self.comment == another.comment)

    def __repr__(self):
        return "%s%s(%s) r%d %s: \"%s\"" % (repr(self.operation), self.document, datetime.strftime(self.date, "%Y-%m-%d %H:%M:%S"), self.revision_id, self.ip or self.author_name, self.comment)






class SeperationException(Exception):
    pass


class Seperator(object):
    def __init__(self, s):
        self.s = s
        self.idx = 0

    def pop(self, *sigs):
        if len(sigs) == 0:
            raise ValueError("At least one signature should be given")

        result = []
        if len(sigs) == 1:
            iter_idx = self.idx
        else:
            iter_idx = self.s.find(sigs[0], self.idx)
            if iter_idx == -1:
                raise SeperationException
            iter_idx += len(sigs[0])
            sigs = sigs[1:]

        for sig in sigs:
            sig_idx = self.s.find(sig, iter_idx)
            if sig_idx == -1:
                raise SeperationException
            result.append(self.s[iter_idx : sig_idx])
            iter_idx = sig_idx + len(sig)
        self.idx = iter_idx

        if len(result) == 1:
            return result[0]
        else:
            return tuple(result)

    def get_index(self):
        return self.idx


def retrieve_history(document, max_revision_id=None):
    if isinstance(document, unicode):
        document = document.encode("UTF-8")

    url = "https://namu.wiki/history/"
    url += "/".join([quote(chunk) for chunk in document.split('/')])
    if max_revision_id is not None:
        url += "?rev=%d"%max_revision_id

    src = spoofing_urlopen(url)

    main_sep = Seperator(src)

    try:
        _, _, ul_content = main_sep.pop("id=\"diffbtn\"", ">", "<ul>", "</ul>")
        result = []
        for line in split_li(ul_content):
            line_sep = Seperator(line)
            date_str = line_sep.pop("<span").strip()
            date = datetime.strptime(date_str,"%Y-%m-%d %H:%M:%S")

            try:
                operation_str = line_sep.pop("<i>", "</i>").strip()
                operation = _interpret_i(operation_str)
            except SeperationException:
                operation = DocumentOperation.modify()
            revision_str = line_sep.pop("<strong>", "</strong>").strip()
            if not revision_str.startswith("r"):
                raise FormatMayHaveChangedException
            try:
                revision_id = int(revision_str[1:])
            except ValueError:
                raise FormatMayHaveChangedException

            ctrbn_sig = line_sep.pop("<a href=\"/contribution/", "/")
            ctrbn_author = unescape_html_entity(line_sep.pop(">", "<")).strip()

            _, _, comment, _ = line_sep.pop("(", "<span", ">", "</span>", ")")
            comment = unescape_html_entity(comment).strip()
            ip = None
            author_name = None
            if ctrbn_sig == "ip":
                ip = ctrbn_author
            else:
                author_name = ctrbn_author
            result.append(HistoryRecord(document, date, operation, revision_id, ip, author_name, comment))
    except SeperationException as err:
        raise
    return result


def retrieve_recent_changes():
    src = spoofing_urlopen("https://namu.wiki/RecentChanges")

    main_sep = Seperator(src)

    tbody = main_sep.pop("<tbody>", "</tbody>")

    idx = 0
    trs = split_li(tbody, "<tr>", "</tr>")
    length = len(trs)

    result = []
    while idx < length:
        tr_content = trs[idx]  

        tr_sep = Seperator(tr_content)

        tr_sep.pop("<td>")
        try:
            operation_str = tr_sep.pop("<i>", "</i>").strip()
            operation = _interpret_i(operation_str, ro_exists=False)
        except SeperationException:
            operation = DocumentOperation.modify()

        _, _, document = tr_sep.pop("<a href=", "w/", ">", "</a>")
        document = unescape_html_entity(document).strip()
        
        if operation.op != DocumentOperation.NEW:
            _, _, rev_id_str, _ = tr_sep.pop("<a href=", "diff/", "?rev=", "&", "</a>")
            try:
                revision_id = int(rev_id_str)
            except ValueError:
                raise FormatMayHaveChangedException
            tr_sep.pop("</td>")
        else:
            revision_id = 1

        operation.current_document = document

        _, ctrbn_sig, _, ctrbn_author, _ = tr_sep.pop("<td>", '<a href="/contribution/', "/", ">", "</a>", "</td>")
        ctrbn_author = unescape_html_entity(ctrbn_author).strip()

        date_str = tr_sep.pop("<td>", "</td>").strip()
        date = datetime.strptime(date_str,"%Y-%m-%d %H:%M:%S")

        if idx + 1 < length and trs[idx + 1].count("</td>") == 1:
            _, comment, = Seperator(trs[idx + 1]).pop("<td", ">", "</td>")
            idx += 2
        else:
            comment = ""
            idx += 1
        comment = unescape_html_entity(comment).strip()

        ip = None
        author_name = None
        if ctrbn_sig == "ip":
            ip = ctrbn_author
        else:
            author_name = ctrbn_author
        result.append(RecentChange(document, date, operation, revision_id, ip, author_name, comment))
    return result


def retrieve_raw_document(document, revision_id=None):
    if isinstance(document, unicode):
        document = document.encode("UTF-8")

    tag = ""
    if revision_id is not None:
        tag = "?rev=%d" % revision_id
    url = "https://namu.wiki/raw/"
    url += "/".join([quote(chunk) for chunk in document.split('/')])
    url += tag
    return spoofing_urlopen(url)


def recent_change_difference(old_rc_list, new_rc_list):
    for new_offset in range(len(new_rc_list)):
        old_idx = 0
        old_len = len(old_rc_list)
        new_len = len(new_rc_list)
        success = True
        while old_idx < old_len and old_idx + new_offset < new_len:
            new_idx = old_idx + new_offset 
            if not old_rc_list[old_idx].rc_equal(new_rc_list[new_idx]):
                success = False
                break
            old_idx += 1
        if success:
            return new_rc_list[:new_offset]
    return new_rc_list[:]


def rc_loop():
    old_rc_list = retrieve_recent_changes()
    while True:
        time.sleep(3);
        rc_list = retrieve_recent_changes()

        pprint(recent_change_difference(old_rc_list, rc_list))
        old_rc_list = rc_list


def commentize(cmt):
    cmt = cmt.strip()
    if cmt:
        return cmt.replace(" ", "_")
    else:
        return "_"


def retrieve_whole_history(document, term=1):
    result = retrieve_history(document)
    least_recent_revision_id = result[-1].revision_id
    while least_recent_revision_id > 1:
        time.sleep(term)
        result.extend(retrieve_history(document, least_recent_revision_id - 1))
        least_recent_revision_id = result[-1].revision_id
    return result


if __name__ == '__main__':
    pprint(retrieve_recent_changes())
