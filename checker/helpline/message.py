import random
import time
from enum import Enum

def escape(str, extra=":"):
    """
    ExtModule protocol escape.
    """
    str = str + ""
    s = ""
    n = len(str)
    i = 0
    while i < n:
        c = str[i]
        if (ord(c) < 32) or (c == extra):
            c = chr(ord(c) + 64)
            s = s + "%"
        elif (c == "%"):
            s = s + c
        s = s + c
        i = i + 1
    return s


def unescape(str):
    """
    ExtModule protocol unescape.
    """
    s = ""
    n = len(str)
    i = 0
    while i < n:
        c = str[i]
        if c == "%":
            i = i + 1
            c = str[i]
            if c != "%":
                c = chr(ord(c) - 64)
        s = s + c
        i = i + 1
    return s

class MessageType(Enum):
    MESSAGE = 1,
    RESPONSE = 2,
    WATCH = 3,
    INSTALL_RESPONSE = 4,
    WATCH_RESPONSE = 5

class YATEMessage():
    def __init__(self,
                 mid=None,
                 name=None,
                 timestamp=None,
                 retvalue=None,
                 attrs=None,
                 processed=False,
                 returned=False,
                 type=MessageType.MESSAGE):

        self._name = name
        self._type = type

        if timestamp:
            self._timestamp = timestamp
        else:
            self._timestamp = int(time.time())

        if mid:
            self._mid = mid
        else:
            self._mid = str(self._timestamp) + str(random.randrange(1, 10000, 1))

        self._returned = returned
        self._retvalue = retvalue
        self._attrs = attrs
        self._processed = processed

    def __str__(self):
        result = self.getName()
        for k in self:
            result = result + ", " + "%s=%s" % (k, self[k])
        return result

    def _format_attrs(self):
        """
        Format message attributes.
        """
        result = ""
        for name, value in self._attrs.items():
            if type(value) == type(""):
                v = value.encode('raw_unicode_escape')
            elif type(value) != type(""):
                v = str(value)
            else:
                v = value
            result = result + ":" + name + "=" + escape(str(value))
        return result

    def _format_message_response(self, returned):
        result = "%%%%<message:%s:%s" % (self._mid, str(returned).lower())

        result = result + ":" + escape(self._name)

        if self._retvalue:
            result = result + ":" + escape(str(self._retvalue))
        else:
            result = result + ":"
        result = result + self._format_attrs()

        return result

    def format_response(self, handled=True, retValue=None):
        if retValue:
            self.setRetValue(retValue)
        resp = self._format_message_response(handled)
        self._returned = True
        return resp

    def format_message(self):
        """
        %%>message:<id>:<time>:<name>:<retvalue>[:<key>=<value>...]
        """

        result = "%%%%>message:%s:%s" % (self._mid, str(self._timestamp))
        result = result + ":" + escape(self._name)
        result = result + ":"
        if self._retvalue:
            result = result + escape(self._retvalue)
        return result + self._format_attrs()

    def getName(self):
        return self._name

    def __getitem__(self, key):
        return self._attrs.get(key, None)

    def __setitem__(self, key, value):
        self._attrs[key] = value

    def __iter__(self):
        return self._attrs.__iter__()

    def setRetValue(self, value):
        self._retvalue = value

    def getRetValue(self):
        return self._retvalue

class MessageParser():
    def __init__(self):
        self.parsers = {"%%>message": self._messageReceived,
                        "%%<message": self._watchOrResponseReceived,
                        "%%<install": self._installResponse,
                        "%%<watch": self._watchResponse}
        pass

    def _watchOrResponseReceived(self, values):
        values = values.split(':', 4)
        if values[0] == "":
            return self._watchReceived(values)
        else:
            return self._messageResponse(values)

    def _parse_attrs(self, values):
        values = values.split(":")
        if values:
            attrs = {}
            for key, value in [x.split("=", 1) for x in values]:
                attrs[key] = unescape(value)
            return attrs
        else:
            return None

    def _messageReceived(self, values):
        values = values.split(':', 4)

        m = YATEMessage(
            mid=unescape(values[0]),
            timestamp=values[1],
            name=unescape(values[2]),
            retvalue=None,
            attrs=None,
            type=MessageType.MESSAGE)

        l = len(values)
        if l > 3: m.setRetValue(unescape(values[3]))
        if l > 4: m._attrs = self._parse_attrs(values[4])

        return m

    def _watchReceived(self, values):
        w = YATEMessage(
            mid=unescape(values[0]),
            returned=values[1],
            name=unescape(values[2]),
            retvalue=unescape(values[3]),
            attrs=self._parse_attrs(values[4]),
            type=MessageType.WATCH)
        
        return w

    def _messageResponse(self, values):
        #print("Received response.")
        #print(values)

        if values[1] in ("true", "ok"):
            processed = True
        else:
            processed = False

        m = YATEMessage(
            mid=values[0],
            name=values[2],
            retvalue=values[3],
            attrs=self._parse_attrs(values[4]),
            type=MessageType.RESPONSE)
        
        return m
#        
#        w = YATEMessage(
#            mid=unescape(values[0]),
#            processed=processed,
#            returned=values[1],
#            name=unescape(values[2]),
#            retvalue=unescape(values[3]),
#            attrs=self._parse_attrs(values[4]))
#        pass
#        mid = values[0]
#        if mid in self.waiting:
#            m, d = self.waiting[mid]
#            l = len(values)
#            if len(values) > 3:
#                m.setRetValue(unescape(values[3]))
#            if len(values) > 4:
#                m._attrs = self._parse_attrs(values[4])
#            del self.waiting[mid]
#
#            if logger_messages.isEnabledFor(logging.DEBUG):
#                logger_messages.debug("received response:\n" + str(m))
#
#            d.callback(values[1] == "true")
#        else:
#            if logger.isEnabledFor(logging.WARN):
#                logger.warn("Response to unknown message: %s", str(values))

    def _watchResponse(self, values):
        values = values.split(':')
        if not unescape(values[1]) in ("ok", "true"):
            raise Exception("Can't install handler for: %s", str(values[0]))
        return YATEMessage(type=MessageType.WATCH_RESPONSE)

    def _installResponse(self, values):
        values = values.split(':')
        if not unescape(values[2]) in ("ok", "true"):
            raise Exception("Can't install handler for: %s" % str(values[1]))
        return YATEMessage(type=MessageType.INSTALL_RESPONSE)
    
    def parse_message(self, line):
        #print("Parsing line '%s'" % line)
        if line == "":
            raise Exception("Can't build message from empty string!")
        line = line.rstrip()
        if line.startswith("Error in:"):
            raise Exception(line)

        kind, rest = line.split(':', 1)

        parser = self.parsers.get(kind, None)
        if parser:
            return parser(rest)
        else:
            raise Exception("Can't interpret line: " + line)