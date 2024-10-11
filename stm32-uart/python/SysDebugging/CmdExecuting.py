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

from Processing import *

class CmdExecuting(Processing):

	def cmdSet(self, cmd):
		self.cmd = cmd

	def initialize(self):

		self.state = self.buffOutCmdValidWait
		self.resp = ""
		self.msStart = time_ns() // 10**6

		return Positive

	def process(self):

		return self.state()

	def buffOutCmdValidWait(self):

		msCurTime = time_ns() // 10**6

		if msCurTime - self.msStart > 500:
			self.procDbgLog("Timeout reached while starting command")
			return -1

		if not "cmd" in aEnv.dataOut:
			aEnv.dataOut["cmd"] = {}
			aEnv.dataOut["cmd"]["id"] = 0
			aEnv.dataOut["cmd"]["name"] = ""

		if aEnv.dataOut["cmd"]["name"] != "":
			return Pending

		self.procDbgLog("Waited for command buffer to be empty. Setting command")

		aEnv.dataOut["cmd"]["id"] = aEnv.dataOut["cmd"]["id"] + 1
		self.cmdId = aEnv.dataOut["cmd"]["id"]
		aEnv.dataOut["cmd"]["name"] = self.cmd

		self.msStart = time_ns() // 10**6

		self.state = self.buffInCmdValidWait

		return Pending

	def buffInCmdValidWait(self):

		msCurTime = time_ns() // 10**6

		if msCurTime - self.msStart > 3000:
			self.procDbgLog("Timeout reached for command execution")
			aEnv.dataOut["cmd"]["name"] = ""
			return -2

		if not "cmd" in aEnv.dataIn:
			return Pending

		if not "id" in aEnv.dataIn["cmd"]:
			return Pending

		if aEnv.dataIn["cmd"]["id"] != self.cmdId:
			return Pending

		self.procDbgLog("Got command response. Command execution done")

		self.resp = aEnv.dataIn["cmd"]["resp"]
		self.procDbgLog("Response: %s" % self.resp)

		aEnv.dataOut["cmd"]["name"] = ""

		return Positive

