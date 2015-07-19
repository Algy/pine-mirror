#!/usr/bin/env python
# -*- coding: utf-8 -*-

import gzip
import time

from docdata import HistoryRecord, DocumentOperation, RecentChange
from datetime import datetime
from StringIO import StringIO
from urllib2 import Request, urlopen, quote, HTTPError
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


def _spoofing_content_decoder(headers, raw):
    if headers.get("Content-Encoding", None) == "gzip":
        compressed_stream = StringIO(raw)
        return gzip.GzipFile(fileobj=compressed_stream).read()
    else:
        return raw


class SpoofingUrlopenException(Exception):
    def __init__(self, http_exc):
        Exception.__init__(self, http_exc.args)
        self.code = http_exc.code
        self.msg = http_exc.msg
        self.headers = http_exc.headers
        self.content = _spoofing_content_decoder(http_exc.headers, http_exc.fp.read())
        self.args = ("%d %s"%(self.code, self.msg), )


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
    try:
        resp = urlopen(req)
        return _spoofing_content_decoder(resp.headers, resp.read())
    except HTTPError as exc:
        raise SpoofingUrlopenException(exc)


def split_li(s, st_tag="<li>", ed_tag="</li>"):
    try:
        result = [particle.split(ed_tag, 1)[0] 
                  for particle in filter(bool, [x.strip() for x in s.split(st_tag)])]
        return result
    except IndexError as err:
        raise FormatMayHaveChangedException(err)


def _interpret_i(i, ro_exists=True):
    if isinstance(i, unicode):
        i = i.encode("UTF-8")
    i = i.strip()
    if i == '(\xec\x83\x88 \xeb\xac\xb8\xec\x84\x9c)': # sae moonseo
        return DocumentOperation.create()
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


def retrieve_recent_changes(logtype=None):
    url = "https://namu.wiki/RecentChanges"
    if logtype is not None:
        url += "?logtype=%s"%logtype
    src = spoofing_urlopen(url)
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
        
        if operation.op != DocumentOperation.CREATE:
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
