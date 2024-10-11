#!/usr/bin/python

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

from time import sleep

from Processing import *
from TcpListening import *
from SingleWireTransfering import *
from PeerCmdCommunicating import *

class SwartDebugging(Processing):

	def initialize(self):

		self.procPeers = []
		self.logPeers = []
		self.contentIdLog = 0xC0
		self.contentIdProc = 0xC2

		self.procTree = ""
		self.procTreeUpdated = False
		self.procTreeChangedTimeMs = 0

		self.start(SingleWireTransfering())

		self.logListener = TcpListening()
		self.logListener.portSet(3000)
		self.start(self.logListener)

		self.procListener = TcpListening()
		self.procListener.portSet(3001)
		self.start(self.procListener)

		self.cmdListener = TcpListening()
		self.cmdListener.portSet(3002)
		self.start(self.cmdListener)

		return Positive

	def process(self):

		self.logPeersCommunicate()
		self.procPeersCommunicate()

		peerCmd = self.cmdListener.peerGet()
		if peerCmd != None:
			peerCmdCommunicator = PeerCmdCommunicating()
			peerCmdCommunicator.peerSet(peerCmd)
			self.start(peerCmdCommunicator)

		return Pending

	def logPeersCommunicate(self):

		peerLog = self.logListener.peerGet()
		if peerLog != None:
			self.procDbgLog("Adding log peer")
			self.logPeers.append(peerLog)
			self.procDbgLog("Adding log peer: done")

		if not self.contentIdLog in aEnv.dataIn:
			return

		if aEnv.dataIn[self.contentIdLog].empty():
			return

		msg = aEnv.dataIn[self.contentIdLog].get() + "\n"

		for peerLog in self.logPeers:
			try:
				peerLog[0].send(msg.encode("utf-8"))
			except BrokenPipeError:
				self.procDbgLog("Removing log peer")
				self.logPeers.remove(peerLog)
				self.procDbgLog("Removing log peer: done")

	def procPeersCommunicate(self):

		peerProc = self.procListener.peerGet()
		if peerProc != None:
			self.procDbgLog("Adding proc peer")
			self.procPeers.append(peerProc)
			self.procDbgLog("Adding proc peer: done")

			if len(self.procTree):
				msg = "\033[2J\033[H\n" + self.procTree
				peerProc[0].send(msg.encode("utf-8"))

		if not self.contentIdProc in aEnv.dataIn:
			return

		if aEnv.dataIn[self.contentIdProc].empty():
			return

		dataIn = aEnv.dataIn[self.contentIdProc].get()
		msg = "\033[2J\033[HProcess tree size: " + str(len(dataIn)) + "\n\n" + dataIn

		msCurTime = time_ns() // 10**6

		if self.procTreeUpdated:
			if msCurTime - self.procTreeChangedTimeMs < 50:
				return

			self.procTreeUpdated = False

		if self.procTree == msg:
			return

		for peerProc in self.procPeers:
			try:
				peerProc[0].send(msg.encode("utf-8"))
			except:
				self.procDbgLog("Removing proc peer")
				self.procPeers.remove(peerProc)
				self.procDbgLog("Removing proc peer: done")

		self.procTree = msg
		self.procTreeUpdated = True
		self.procTreeChangedTimeMs = msCurTime

if __name__ == "__main__":

	parser = argparse.ArgumentParser(description = 'Debugging via single wire interface')
	parser.add_argument('-c', '--channel', type = str, help = 'Debug channel: tty (default), socket', required = False, default = 'tty')
	parser.add_argument('-p', '--port', type = int, help = 'Port', required = False, default = 2000)
	args = parser.parse_args()

	aEnv.dbgChannel = args.channel
	aEnv.dbgPort = args.port

	pApp = SwartDebugging()

	while True:
		pApp.rootTick(None)
		sleep(0.01)

