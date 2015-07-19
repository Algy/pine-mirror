# -*- coding: utf-8 -*-

from datetime import datetime

class DocumentOperation(object):
    CREATE = 'create'
    MODIFY = 'modify'
    DELETE = 'delete'
    REVERT = 'revert'
    MOVE = 'move'
    OPERATIONS = [CREATE, 
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
                self.original_document == another.original_document)

    def __repr__(self):
        if self.op == self.REVERT:
            return "[%s to r%d]"%(self.op, self.revision_id)
        elif self.op == self.MOVE:
            return "[%s from '%s' to '%s']"%(self.op, self.original_document, self.current_document)
        else:
            return "[%s]"%self.op


    @classmethod
    def create(cls):
        return cls(cls.CREATE)

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


class HistoryRecord(object):
    def __init__(self, document, date, operation, revision_id, ip=None, author_name=None, comment=""):
        assert (isinstance(revision_id, (int, long,)))
        assert (isinstance(date, datetime))
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
        assert (isinstance(date, datetime))
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


def recent_change_difference(old_rc_list, new_rc_list):
    old_len = len(old_rc_list)
    new_len = len(new_rc_list)
    if old_len == 0:
        return new_rc_list[:]
    for new_offset in range(len(new_rc_list)):
        old_idx = 0
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

