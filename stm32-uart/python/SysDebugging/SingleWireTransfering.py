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

import aEnv

import os
import termios
import tty
import fcntl
import struct
import queue
import socket

from Processing import *

FlowMasterSlave = 0xF0
FlowSlaveMaster = 0xF1

ContentOutCmd = 0xC0

ContentInCmd = 0xC1
ContentInNone = 0x00

DataEnd = 0x00 # String termination
DataCut = 0x17 # End of transmittion block

class SingleWireTransfering(Processing):

	def initialize(self):

		if aEnv.dbgChannel == 'socket':
			self.firstRcvState = self.ContentByteRcv
		else:
			self.firstRcvState = self.FlowControlByteRcv

		# Internal
		self.logLevel = 0
		self.msStart = 0
		self.msLastReceived = 0
		self.frameDone = 0
		self.cmdIdOld = 0
		self.stateSend = self.DbgIfInit
		self.stateRcv = self.firstRcvState
		self.fragments = {}
		self.readBlockSize = 4096

		aEnv.dataIn["cmd"] = {}
		aEnv.dataIn["cmd"]["id"] = 0
		aEnv.dataIn["cmd"]["name"] = ""

		# Environment
		aEnv.devOnline = False

		if aEnv.dbgChannel == 'socket':
			return self.socketCreate()
		else:
			return self.ttyCreate()

	def socketCreate(self):
		self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
		self.sock.connect(('localhost', aEnv.dbgPort))
		self.sock.setblocking(False)

		return Positive

	def ttyCreate(self):
		self.TIOCM_RTS_str = struct.pack('I', termios.TIOCM_RTS)
		self.fd = os.open("/dev/ttyUSB0", os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)

		old = termios.tcgetattr(self.fd)
		new = old

		# http://man7.org/linux/man-pages/man3/termios.3.html
		new[tty.IFLAG] &= ~termios.ICRNL

		new[tty.OFLAG] &= ~termios.ONLCR

		new[tty.LFLAG] &= ~termios.ECHO
		new[tty.LFLAG] &= ~termios.ICANON

		new[tty.CFLAG] &= ~termios.CBAUD
		new[tty.CFLAG] |= termios.CBAUDEX

		new[tty.ISPEED] = termios.B115200
		new[tty.OSPEED] = termios.B115200

		try:
			termios.tcsetattr(self.fd, termios.TCSANOW, new)
		except:
			termios.tcsetattr(self.fd, termios.TCSANOW, old)
			return self.procErrLog(-1, "tcsetattr() failed")

		return Positive

	def process(self):

		self.stateSend()

		try:
			if aEnv.dbgChannel == 'socket':
				data = self.sock.recv(self.readBlockSize)
			else:
				data = os.read(self.fd, self.readBlockSize)
		except BlockingIOError:
			return Pending

		#if len(data) > 2:
		#	self.procDbgLog("RxD: Received data. len = %d" % len(data))

		for d in data:
			#self.procDbgLog("RxD: Received data: 0x%02X %c" % (d, chr(d)))
			self.data = d
			self.stateRcv()

		return Pending

	# SEND STATES
	def DbgIfInit(self):

		self.procDbgLog("TxD: Initializing debug interface")
		self.cmdSend("aaaaa")

		self.stateSend = self.NextFlowDetermine

	def NextFlowDetermine(self):

		if "cmd" in aEnv.dataOut and aEnv.dataOut["cmd"]["id"] != self.cmdIdOld:
			self.procDbgLog("TxD: Command received from peer")

			self.cmdSend(aEnv.dataOut["cmd"]["name"])
			self.cmdIdOld = aEnv.dataOut["cmd"]["id"]
		else:
			self.dataSend(bytes([FlowSlaveMaster]))

			self.msLastReceived = time_ns() // 10**6
			self.frameDone = 0

			#self.procDbgLog("TxD: Waiting for response")
			self.stateSend = self.ResponseWait

	def ResponseWait(self):

		msCurTime = time_ns() // 10**6

		if msCurTime - self.msLastReceived > 500:
			self.procDbgLog("TxD: Timeout reached for single wire transfer")

			aEnv.devOnline = False
			self.stateRcv = self.firstRcvState

			self.procDbgLog("TxD: Waiting for re-init")
			self.msStart = msCurTime

			self.stateSend = self.ReInitWait
			return

		if not self.frameDone:
			return

		aEnv.devOnline = True
		self.stateSend = self.NextFlowDetermine

	def ReInitWait(self):

		msCurTime = time_ns() // 10**6

		if msCurTime - self.msStart < 1500:
			return

		self.procDbgLog("TxD: Waiting for re-init: done")

		self.stateSend = self.DbgIfInit

	def cmdSend(self, cmd):

		try:
			self.dataSend(bytes([FlowMasterSlave]))
			self.dataSend(bytes([ContentOutCmd]))
			self.dataSend(str.encode(cmd))
			self.dataSend(b"\x00")
		except Exception as e:
			self.procDbgLog(repr(e))

	def dataSend(self, d):
		if aEnv.dbgChannel == 'socket':
			self.sock.send(d)
		else:
			os.write(self.fd, d)
	# END SEND STATES

	# RCV STATES
	def FlowControlByteRcv(self):

		if self.data == FlowMasterSlave:
			self.stateRcv = self.DataIgnore
			self.procDbgLog("RxD: Ignoring data")
		elif self.data == FlowSlaveMaster:
			self.stateRcv = self.ContentByteRcv
			#self.procDbgLog("RxD: Receiving data")

	def DataIgnore(self):

		if self.data == DataEnd or self.data == DataCut:
			self.stateRcv = self.firstRcvState
			self.procDbgLog("RxD: Ignoring data: done")

	def ContentByteRcv(self):

		self.msLastReceived = time_ns() // 10**6

		self.contentId = self.data

		if self.data == ContentInNone:
			self.frameDone = 1
			self.stateRcv = self.firstRcvState
			#self.procDbgLog("RxD: Receiving data: done")
		else:
			if self.contentId != 0xC2:
				self.procDbgLog("RxD: Received content ID from device: 0x%02X" % self.contentId)

			self.stateRcv = self.DataRcv

	def DataRcv(self):

		self.msLastReceived = time_ns() // 10**6

		if self.data == DataEnd:
			if not self.contentId in self.fragments:
				self.procDbgLog("RxD: Got empty message")

				self.stateRcv = self.firstRcvState
				return

			if not self.contentId in aEnv.dataIn:
				aEnv.dataIn[self.contentId] = queue.Queue()

			resp = self.fragments[self.contentId]

			aEnv.dataIn[self.contentId].put(resp)
			del self.fragments[self.contentId]

			if self.contentId == ContentInCmd:
				resp = aEnv.dataIn[self.contentId].get()

				self.procDbgLog("RxD: %s" % resp)

				if self.cmdIdOld != aEnv.dataIn["cmd"]["id"]:
					aEnv.dataIn["cmd"]["id"] = self.cmdIdOld
					aEnv.dataIn["cmd"]["resp"] = resp
				else:
					self.procDbgLog("RxD: Ignored received command response")

			if self.contentId != 0xC2:
				self.procDbgLog("RxD: Receiving data: done")

			self.frameDone = 1
			self.stateRcv = self.firstRcvState
			return

		if self.data == DataCut:
			self.procDbgLog("RxD: Receiving data fragment: done")

			self.frameDone = 1
			self.stateRcv = self.firstRcvState
			return

		if not self.contentId in self.fragments:
			self.fragments[self.contentId] = ""

		self.fragments[self.contentId] += chr(self.data)
		#self.procDbgLog("RxD: Fragment %s" % self.fragments[self.contentId])
	# END RCV STATES

