#  This file is part of the DSP-Crowd project
#  https://www.dsp-crowd.com
#
#  Author(s):
#      - Johannes Natter, office@dsp-crowd.com
#
#  File created on 30.11.2019
#
#  Copyright (C) 2019, Johannes Natter
#
#  Permission is hereby granted, free of charge, to any person obtaining a copy
#  of this software and associated documentation files (the "Software"), to deal
#  in the Software without restriction, including without limitation the rights
#  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
#  copies of the Software, and to permit persons to whom the Software is
#  furnished to do so, subject to the following conditions:
#
#  The above copyright notice and this permission notice shall be included in all
#  copies or substantial portions of the Software.
#
#  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
#  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
#  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
#  SOFTWARE.

import select

from Processing import *
from CmdExecuting import *

class PeerCmdCommunicating(Processing):

	def peerSet(self, peer):

		self.peer = peer
		sock = self.peer[0]
		sock.send("# ".encode("utf-8"))

	def initialize(self):

		self.procDbgLog("Peer command %s:%s added" % self.peer[1])
		self.state = self.cmdReceive
		self.lastCmd = ""

		return Positive

	def process(self):

		return self.state()

	def cmdReceive(self):

		sock = self.peer[0]

		sockRdOk, sockWrOk, sockErr = select.select([sock], [], [], 0.005)

		if not sockRdOk:
			return Pending

		data = sock.recv(1024)

		if not len(data):
			self.procDbgLog("Peer command %s:%s removed" % self.peer[1])
			return Positive

		dataStr = str.rstrip(data.decode("utf-8"))
		self.procDbgLog("Received: %s (%d)" % (dataStr, len(dataStr)))

		if not len(dataStr):
			dataStr = self.lastCmd

		if not len(dataStr):
			sock.send("# ".encode("utf-8"))
			return Pending

		self.lastCmd = dataStr

		self.procDbgLog("Creating executor")
		self.cmdExecutor = CmdExecuting()
		self.cmdExecutor.cmdSet(dataStr)
		self.start(self.cmdExecutor)

		self.state = self.cmdWait

		return Pending

	def cmdWait(self):

		if self.cmdExecutor.success == Pending:
			return Pending

		sock = self.peer[0]

		# Send response back
		self.procDbgLog("Executor finished. Sending response")

		if self.cmdExecutor.success == Positive:
			resp = self.cmdExecutor.resp + "\n"
			sock.send(resp.encode("utf-8"))
		else:
			sock.send("Error executing command\n".encode("utf-8"))

		sock.send("# ".encode("utf-8"))

		self.procDbgLog("Deleting executor")
		#self.delProc(self.cmdExecutor)

		self.state = self.cmdReceive

		return Pending

