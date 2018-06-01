#!/usr/bin/python

from twisted.internet import reactor, defer
from yaypm import TCPDispatcherFactory
from yaypm.flow import go, getResult
import logging, yaypm.utils
from yaypm.utils import ConsoleFormatter

logger = logging.getLogger('yaypm.examples')

#class clientConnector(object):
#    def __init__(self, client):
#        print("Connector/init()")
#        self.client = client
#        go(self.connect())
#
#    def connect(self):
#        yield self.client.installWatchHandler("call.answered")
#        getResult()
#        print("Connecting!")
#        execute = self.client.msg("call.execute",
#                {"callto": "line/017656102342",
#                 "caller" : "iridium",
#                 "target": "iridium"
#                })
#        yield execute.dispatch()
#        #print("Dispatch")
#        #t = defer.waitForDeferred(execute.dispatch())
#        #print("Yield")
#        #yield t
#        #print("Returned from yield")
#        #t = t.getResult()
#        if not getResult():
#            print("No result?")
#        else:
#            print("Yes :-)")
#        yield self.client.msg("call.drop", {"id": execute["targetid"]})

def start_client(client_yate):
    print("client started")
    clientConnector(client_yate)

def main(yate):
#    print "Received message :-)"
#    execute = getResult()
#    print "Result"
#    if not execute:
#        print "...broken"
#    else:
#        print "OK"
#    yield yate.installMsgHandler("call.route", prio = 50)
#    if not getResult():
#        print "WTF"
#    print "Waiting!"
#    yield yate.onmsg("call.route", lambda m : True)
#    print "Got result!"
#    test = getResult()
#    if not getResult():
#        print "WTF2"
    try:
#        print "Printing!"
#        print "Message: %s %s %s" % (test["caller"], test["callername"], test["called"])
#        print "Direct: %s" % (test["direct"])
        execute = yate.msg("call.execute",
                {"callto":"dumb/",
                 "caller":"crypto_addict",
                 "target":"100"})
        yield execute.dispatch()
        ex_r = getResult()
        if not ex_r:
            print "FAILURE"
        else:
            print "SUCCESS"
        dtmf = yate.msg("chan.dtmf",
                {"targetid":ex_r["id"],
                 "text":"1234"})
        yield dtmf.dispatch()
        if not getResult():
            print "FAILURE"
        else:
            print "SUCCESS"
    except BaseException as e:
        logging.exception("foo")
    #sys.exit(0)
    #logger.debug("Client started")

   # route = yate.msg("call.execute",
   #         {"callto":"017656102342",
   #          "caller":"091318119643",
   #          "callername":"iridium",
   #          "called":"017656102342",
   #          })
#  #          {"target": "017656102342"})
#, #"callername": "iridium", "caller": "iridium"})
   # yield route.dispatch()
   # if not getResult():
   #     print("Could not call!")
   # else:
   #     print("Success!")
   # execute = yate.msg(
   #     "call.execute",
   #     {"callto": "line/017656102342",
   #      "caller": "iridium"})
   # yield execute.dispatch()
   # if not getResult():
   #     print("Could not call!")
   # else:
   #     print("Success!")

if __name__ == "__main__":
    #print "hi"
    hdlr = logging.StreamHandler()
    formatter = ConsoleFormatter('%(name)s %(levelname)s %(message)s')
    hdlr.setFormatter(formatter)

    yaypm.logger.addHandler(hdlr)
    yaypm.logger.setLevel(logging.DEBUG)
    #yaypm.flow.logger_flow.addHandler(hdlr)
    #yaypm.flow.logger_flow.setLevel(logging.DEBUG)
    yaypm.logger_messages.setLevel(logging.DEBUG)
    #yaypm.flow.logger_messages.addHandler(hdlr)
    #yaypm.logger_messages.setLevel(logging.INFO)

    logger.setLevel(logging.DEBUG)
    logger.addHandler(hdlr)

    client_factory = TCPDispatcherFactory(lambda yate: go(main(yate)))
    reactor.connectTCP('127.0.0.1', 5039, client_factory)
    reactor.run()
