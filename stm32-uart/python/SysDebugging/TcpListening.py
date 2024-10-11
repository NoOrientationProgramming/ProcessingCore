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

import queue
import socket
import select

from Processing import *

class TcpListening(Processing):

	def initialize(self):

		self.logLevel = 0
		self.peers = queue.Queue()

		if self.port == None:
			self.port = 3002

		self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
		self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
		self.sock.setblocking(0)

		sockLocal = ("0.0.0.0", self.port)
		self.sock.bind(sockLocal)

		self.sock.listen(5)

		self.procDbgLog("Listening on 0.0.0.0:" + str(self.port))

		return Positive

	def process(self):

		sockRdOk, sockWrOk, sockErr = select.select([self.sock], [], [], 0.005)

		if not sockRdOk:
			return Pending

		sockPeer = self.sock.accept()

		#self.procDbgLog("Client connected %s:%s" % sockPeer[1])

		self.peers.put(sockPeer)

		return Pending

	def portSet(self, port):

		self.port = port

	def peerGet(self):

		if self.peers.empty():
			return None

		return self.peers.get()

