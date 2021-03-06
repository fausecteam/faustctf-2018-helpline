# Checker for service 'helpline' at FAUST CTF 2018
# Copyright (c) 2018 Dominik Paulus <faust-ctf@dominik-paulus.de>
# Distributed under the terms of the GNU General Public License v2

# Checker performance sucks?
# Things to remove:
#  a) _check_audio() in HelplineChecker
#     This check checks whether the audio output (text-to-speech)
#     sounds somehow reasonable.
#     It is "somehow" resource-intensive (invokes scipy/FFT) on
#     inputs.
#     You can simply replace the whole function by 'return OK'
#     without breaking the rest of the checker.
#  b) _check_place_retrieve() in HelplineChecker
#     Possible code changes are clearly marked as such.
#     Removing them removes the check if the phone number
#     can be changed.

from ctf_gameserver.checker import BaseChecker, OK, NOTWORKING, TIMEOUT, NOTFOUND
from yaypm import TCPDispatcherFactory
from yaypm.utils import XOR, OR, sleep
from twisted.internet import reactor, defer
import twisted.python
from threading import Thread
from scipy import fft
#from pylab import plot, show, title, xlabel, ylabel, subplot
import threading, queue, random, logging, tempfile
import yaypm.utils, subprocess
import scipy, scipy.io, scipy.io.wavfile, scipy.fftpack
#import matplotlib.pyplot as plt
import numpy

timeout = 15

class HelplineChecker(BaseChecker):
    # List of available tones in DTMF
    _characters = ['0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', '*']

    def __init__(self, tick, team, service, ip):
        BaseChecker.__init__(self, tick, team, service, ip)
        self._tick = tick
        self._team = team
        self._service = service
        self._ip = ip
        self._port = 5743
        self._sock = None

        self._connection = YATEConnection(self.logger)

        self._lock = defer.DeferredLock()

        observer = twisted.python.log.PythonLoggingObserver()
        observer.start()

        #logger = logging.getLogger('yaypm.messages')
        #logger.setLevel(logging.DEBUG)

        flag = "FAUST_VbDgYwwNs6w3AwAAAABEFEtvyHhdDRuN"
        assert flag == self._convert_DTMF_Flag(self._convert_Flag_DTMF(flag))

    def run(self):
        try:
            return super().run()
        finally:
            self._connection.shutdown()

    # Maps flag to DTMF-character-set
    def _convert_Flag_DTMF(self, flag):
        ret = ""
        for c in flag:
            a = ord(c) // len(self._characters)
            b = ord(c) %  len(self._characters)
            ret = ret + self._characters[a] + self._characters[b]
        return ret

    # Maps DTMF-input to flag
    def _convert_DTMF_Flag(self, cc):
        ret = ""
        for c in range(len(cc)//2):
            a = chr(self._characters.index(cc[2*c]) * len(self._characters) +
                self._characters.index(cc[2*c+1]))
            ret = ret + a
        return ret

    def _check_audio(self):
        return OK
        # Simply check whether the first IVR announcement plays back correctly
        @defer.inlineCallbacks
        def check_audio(conn):
            try:
                yield sleep(5)
                yield conn.send_dtmf(random.choice("23456789")) # Compelled
                yield sleep(5)
                yield conn.send_dtmf("2")
                if (yield conn.await_disconnect()):
                    return OK
                else:
                    return NOTWORKING
            except Exception as e:
                print(e)

        ret = self._connection.run(check_audio, self._ip)
        if not ret == OK:
            return ret
        wavfile_f = tempfile.NamedTemporaryFile(delete=False, suffix='.wav')
        wavfile = wavfile_f.name
        wavfile_f.close()

        # Convert 8-bit/8khz/1-channel signed linear raw audio
        # to a wavefile that scipy is able to process
        self.logger.info("Converting audio in check_service")
        process = subprocess.run(["sox",
            # Input file
            "-e", "signed-integer",
            "-t", "raw",
            "-r", "8000",
            "-b", "16",
            "-c", "1",
            self._connection.recfile,
            # Output file
            wavfile])
        if process.returncode != 0:
            self.logger.critical("Failed to convert audio. Return code: %d, stdout: '%s', stderr: '%s'" %
                    (process.returncode, process.stdout, process.stderr))
            raise Exception("sox failed to convert audio")
        self.logger.info("Finished conversion")

        # Read recorded audio from call
        rate, data = scipy.io.wavfile.read(wavfile)
        self.logger.info("Padding input from %d to %d samples" % (len(data), 100000))
        data = numpy.resize(data, 100000)
        ff = scipy.fftpack.fft(data)
        res = []
        for i in range(100):
            d = 0.0
            for j in range(1000):
                d = d + abs(ff[i*1000 + j])
            res.append(d/1000.0)

        # Now, compare audio with what we expected
        fingerprint = [111887.0, 369956.0, 2955023.0, 4193629.0, 1694374.0, 2287078.0, 2559293.0, 1898549.0, 1403567.0, 1479388.0, 1352175.0, 1221967.0, 1446857.0, 851793.0, 728073.0, 691493.0, 578272.0, 441502.0, 460111.0, 553647.0, 378177.0, 333006.0, 393343.0, 344309.0, 285798.0, 285928.0, 308705.0, 242938.0, 234242.0, 241574.0, 217530.0, 226781.0, 163963.0, 206707.0, 206156.0, 165352.0, 164119.0, 189332.0, 166044.0, 184319.0, 134167.0, 114180.0, 98449.0, 96211.0, 69613.0, 67573.0, 59699.0, 36573.0, 17642.0, 14240.0, 14242.0, 17605.0, 36595.0, 59685.0, 67483.0, 69654.0, 96232.0, 98397.0, 114214.0, 134089.0, 184317.0, 166082.0, 189338.0, 164124.0, 165232.0, 206177.0, 206777.0, 163888.0, 226934.0, 217175.0, 241678.0, 234162.0, 243120.0, 308542.0, 286164.0, 285732.0, 344310.0, 393490.0, 332396.0, 378386.0, 553492.0, 460236.0, 440978.0, 578358.0, 691813.0, 728324.0, 850424.0, 1448415.0, 1220980.0, 1352137.0, 1480205.0, 1403252.0, 1898140.0, 2557891.0, 2285083.0, 1695820.0, 4194672.0, 2955697.0, 371247.0, 111594.0]

        max_diff = 0
        for i in range(50):
            max_diff = max(max_diff, abs(fingerprint[i] - res[i]) / fingerprint[i])
            if max_diff > 0.2:
                self.logger.info("Encountered a difference vs fingerprint of %lf. Returning NOTWORKING" %
                        max_diff)
                return NOTWORKING

        self.logger.info("Service: Audio output is working. Maximum difference in frequency magnitudes: %lf%%",
                max_diff * 100)

        return OK

    def _check_place_retrieve(self):
        self.logger.info("Checking if we can create a new user, change the number and later retrieve it")
        user_id = ''.join(random.choice("0123456789") for _ in range(8,9))
        number_1 = ''.join(random.choice("0123456789") for _ in range(5))
        number_2 = ''.join(random.choice("0123456789") for _ in range(5))
        self.logger.info("For this test run: Using user id: %s, old number (before change): %s, new number (after change): %s" %
            (user_id, number_1, number_2))

        @defer.inlineCallbacks
        def callback_register(conn):
            # Step 1: Register new user

            # There are two possible paths through the IVR. Pick one at
            # random.
            if random.choice("12") == "1":
                yield conn.send_dtmf("1") # Compelled
                yield conn.send_dtmf(random.choice("3456789")) # Hodl
            else:
                yield conn.send_dtmf("1") # Compelled
                yield conn.send_dtmf(random.choice("012")) # Hodl
                yield conn.send_dtmf("1") # Confirm purchase

            yield conn.send_dtmf(user_id) # Userid
            yield conn.send_dtmf("#")
            yield conn.send_dtmf(number_1) # Credit card number
            yield conn.send_dtmf("#")
            yield conn.send_dtmf("1") # Wait for callback

            if (yield conn.await_disconnect()):
                return OK
            else:
                return NOTWORKING

        self.logger.info("Step 1: Register new user")
        ret = self._connection.run(callback_register, self._ip)
        if ret != OK:
            self.logger.info("Register not working")
            return ret

        @defer.inlineCallbacks
        def callback_check_update(conn):
            # Step 2: Check number and update it

            # There are two possible paths through the IVR. Pick one at
            # random.
            if random.choice("12") == "1":
                yield conn.send_dtmf("1") # Compelled
                yield conn.send_dtmf(random.choice("3456789")) # Hodl
            else:
                yield conn.send_dtmf("1") # Compelled
                yield conn.send_dtmf(random.choice("012")) # Hodl
                yield conn.send_dtmf("1") # Confirm purchase

            yield conn.send_dtmf(user_id) # Userid
            yield conn.send_dtmf("#")
            yield conn.send_dtmf("2") # Hear your credit card number

            number_recv = yield conn.recv_dtmf_str(len(number_1))
            self.logger.debug("Received number %s, expected number %s" % (number_recv, number_1))
            if number_recv != number_1:
                return NOTWORKING

            self.logger.info("Receiving number OK. Now changing my number")

            return OK

            ####
            # PERFORMANCE ISSUE? REMOVE THIS (just 'return OK' here)
            ####
            yield conn.send_dtmf("3") # Update your creditcard number
            yield conn.send_dtmf(number_2)
            yield conn.send_dtmf("#")
            yield conn.send_dtmf("2") # Hear your credit card number

            self.logger.info("Changing number OK. Now checking if changed number is stored in DB.")

            number_recv = yield conn.recv_dtmf_str(len(number_2))
            self.logger.debug("Received number %s, expected number %s" % (number_recv, number_2))
            if number_recv != number_2:
                return NOTWORKING

            return OK

        self.logger.info("Step 2: Change credit card number and check it again")
        ret = self._connection.run(callback_check_update, self._ip)
        if ret != OK:
            self.logger.info("Check/update not working")
            return ret

    def check_service(self):
        self._check_audio()
        self._check_place_retrieve()
        return OK

    def check_flag(self, tick):
        self.logger.debug("Checking flag for tick %d" % tick)
        user_id_enc = self.retrieve_blob('userid_%03d' % tick)
        if user_id_enc is None:
            self.logger.error("Unable to find flag-ID for tick %d (flagid_%03d)" % (tick, tick))
            return NOTFOUND
        user_id = user_id_enc.decode()
        self.logger.debug("Userid for this tick is %s" % user_id)

        @defer.inlineCallbacks
        def callback(conn):
            # There are two possible paths through the IVR. Pick one at
            # random.
            if random.choice("12") == "1":
                yield conn.send_dtmf("1") # Compelled
                yield conn.send_dtmf(random.choice("3456789")) # Hodl
            else:
                yield conn.send_dtmf("1") # Compelled
                yield conn.send_dtmf(random.choice("012")) # Hodl
                yield conn.send_dtmf("1") # Confirm purchase

            yield conn.send_dtmf(user_id) # Userid
            yield conn.send_dtmf("#")
            yield conn.send_dtmf("2") # Hear your credit card number

            flag_expected = self._convert_Flag_DTMF(self.get_flag(tick))
            flag_recv = yield conn.recv_dtmf_str(len(flag_expected))

            self.logger.debug("Received flag %s, expected flag %s" % (flag_recv, flag_expected))
            if flag_recv == flag_expected:
                return OK
            else:
                return NOTFOUND

        self.logger.debug("Passing callback to YATEConnection-module")
        return self._connection.run(callback, self._ip)

    def place_flag(self):
        self.logger.debug("Placing flag for tick %d" % self.tick)
        user_len = random.choice([n for n in range(9, 12)])
        user_id = ''.join(random.choice("0123456789") for _ in range(user_len))
        self.store_blob('userid_%03d' % self.tick, user_id.encode())
        self.logger.debug("Userid for this tick is %s" % user_id)
        flag = self.get_flag(self.tick)

        @defer.inlineCallbacks
        def callback(conn):
            self.logger.debug("place_flag() callback")
            # There are two possible paths through the IVR. Pick one at
            # random.
            if random.choice("12") == "1":
                yield conn.send_dtmf("1") # Compelled
                yield conn.send_dtmf(random.choice("3456789")) # Hodl
            else:
                yield conn.send_dtmf("1") # Compelled
                yield conn.send_dtmf(random.choice("012")) # Hodl
                yield conn.send_dtmf("1") # Confirm purchase

            yield conn.send_dtmf(user_id) # Userid
            yield conn.send_dtmf("#")
            yield conn.send_dtmf(self._convert_Flag_DTMF(flag)) # Creditcard number
            yield conn.send_dtmf("#")
            yield conn.send_dtmf("1") # Disconnect

            # Receive flagid and store it
            ch = yield conn.await_dtmf()
            if ch is None:
                return TIMEOUT
            flagid = ""
            while ch != '#':
                flagid = flagid + ch
                ch = yield conn.await_dtmf()
            self.store_blob('flagid_%03d' % self.tick, flagid.encode())
            self.logger.info("Flagid for this tick is %s" % flagid)

            if (yield conn.await_disconnect()):
                return OK
            else:
                return TIMEOUT

        return self._connection.run(callback, self._ip)

class YATEConnection:
    # The semantics of the queue are hacky: It is used to
    # communicate back the result values from the computations
    # performed by the callbacks given to run() - python
    # futures are not intended to be constructed by users...
    def __init__(self, logger):
        self.logger = logger
        logger.debug("Constructing YATEConnection")
        self._queue_results = queue.Queue()
        # Make sure that we block until the reactor has been
        # constructed
        wait = threading.Semaphore(value=0)
        def startup(yate):
            self._yate = yate
            wait.release()
        yaypm.utils.setup(startup, runreactor=False)
        reactor.suggestThreadPoolSize(1)
        logger.debug("Spawning reactor-thread")
        self._thrd = Thread(target=reactor.run, args=(False,)).start()
        logger.debug("Waiting for reactor to start up")
        wait.acquire()
        logger.debug("Finished twisted initialization")

    def shutdown(self):
        reactor.stop()

    def run(self, callback, ip):
        """
        Invoke a callback function from a twisted-reactor-thread
        and return its result to the caller.

        This function is not thread-safe and may not be called
        multiple times in parallel.
        """

        @defer.inlineCallbacks
        def cb():
            self.logger.debug("Running callback in twisted reactor")
            ret = (yield self._connect())
            self.logger.debug("Callback returned, return value: {}".format(repr(ret)))
            self._queue_results.put(ret)

        self._callback = callback
        self._ip = ip
        reactor.callFromThread(cb)
        return self._queue_results.get()

    @defer.inlineCallbacks
    def await_disconnect(self):
        self.logger.debug("Waiting for disconnect, timeout: %d" % timeout)
        ret = yield XOR(self._end, sleep(timeout))
        # ret[0] is index of deferred that has fired
        if ret[0] is 0:
            self.logger.info("Successfully received hangup from remote")
            return True
        else:
            self.logger.info("Timeout waiting for remote to hangup")
            return False

    @defer.inlineCallbacks
    def _connect(self):
        try:
            # Username is SIP caller ID in call message
            username = random.choice([
                "crypto_addict",
                "helpless",
                "test",
                ''.join(random.choice("0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ") for _ in range(random.choice([n for n in range (4, 23)]))),
                ''.join(random.choice("0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ") for _ in range(random.choice([n for n in range (4, 23)])))
                ])

            self.logger.info("Username for this connection is %s" % username)

            # Create temporary file to record audio
            tmpfile = tempfile.NamedTemporaryFile(delete=False)
            self.recfile = tmpfile.name
            tmpfile.close()
            self.logger.info("Recording call audio to file %s" % self.recfile)

            # Ring
            execute = self._yate.msg("call.execute",
                    {"callto":"dumb/",
                     "caller":username,
                     "target":self._ip})
            def answeredWatch(m):
                return execute["id"] == m["targetid"]
            # To avoid race conditions, we actually have
            # to install the message handler *before*
            # initiating the connection.
            answered = self._yate.onwatch("call.answered", answeredWatch)
            yield execute.dispatch()

            self._targetid = execute["targetid"]
            self._id = execute["id"]
            self._dtmf = defer.DeferredQueue()
            self._end = self._yate.onwatch(
                "chan.hangup",
                lambda m, callid = execute["id"] : m["id"] == callid)

            # Attach wavefile consumer to dumb channel
            attach_wave = self._yate.msg("chan.masquerade",
                    {"message":"chan.attach",
                     "id":self._id,
                     "consumer":"wave/record/%s" % self.recfile})
            attach_wave.enqueue()

            # Wait for connection to be established
            self.logger.info("Waiting for connection to be established")
            ret = yield XOR(answered, sleep(timeout))
            # ret[0] is index of deferred that has fired
            if ret[0] is 1:
                return TIMEOUT

            self._yate.onmsg("chan.dtmf", lambda m : m["targetid"] == execute["id"]).addCallback(lambda dtmf : self.dtmf_callback(dtmf))

            self.logger.info("Connection established, running actual callback")
            retval = yield self._callback(self)

            # Disconnect
            yield self._yate.msg("call.drop",
                {"id":execute["targetid"]}).dispatch()

            self.logger.debug("Return value: {}".format(repr(retval)))

            return retval
        except Exception as e:
            self.logger.critical(e)

    def dtmf_callback(self, dtmf):
        self._yate.onmsg("chan.dtmf", lambda m : m["targetid"] == self._id).addCallback(lambda dtmf : self.dtmf_callback(dtmf))
        self._dtmf.put(dtmf["text"])
        self.logger.debug("Received DTMF: %s" % dtmf["text"])
        dtmf.ret(True)

    @defer.inlineCallbacks
    def send_dtmf(self, dtmf):
        self.logger.debug("Sending DTMF: %s" % dtmf)
        for c in dtmf:
            #self.logger.debug("At character: %c" % c)
            #self.logger.info("Target: %s" % self._targetid)
            yield self._yate.msg("chan.dtmf",
                    {"targetid":self._targetid,
                     "text":c,
                     "duration":2000}).dispatch()
            yield sleep(0.2)

    @defer.inlineCallbacks
    def await_dtmf(self):
        self.logger.debug("Waiting for DTMF")
        qget = self._dtmf.get()
        ret = yield XOR(qget, sleep(8))
        if ret[0] is 1:
            self.logger.debug("Timeout")
            return None
        else:
            #self.logger.debug("Success")
            return ret[1]

    @defer.inlineCallbacks
    def recv_dtmf_str(self, length):
        self.logger.debug("Receiving DTMF string")
        str_recv = ""
        for i in range(length):
            _ = i
            dtmf = yield self.await_dtmf()
            if dtmf is None:
                self.logger.info("Receiving DTMF string timeouted. Already received %d DTMF-tones" % len(str_recv))
                return TIMEOUT
            str_recv = str_recv + dtmf
        self.logger.debug("Received string %s", str_recv)
        return str_recv

if __name__ == "__main__":
    @defer.inlineCallbacks
    def callback(conn):
        print("In callback!")
        yield conn.send_dtmf("1943219812#AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA26#")
        print("Now receiving DTMF")
        print("Test: %s" % (yield conn.await_dtmf()))
        print("Test: %s" % (yield conn.await_dtmf()))
        print("Test: %s" % (yield conn.await_dtmf()))
        print("Test: %s" % (yield conn.await_dtmf()))
        print("Test: %s" % (yield conn.await_dtmf()))
        print("Returning")
        return "4242"

    logger = logging.getLogger('default')
    logger.setLevel(logging.DEBUG)
    conn = YATEConnection(logger)
    conn.run(callback, "10.110.110.200")
