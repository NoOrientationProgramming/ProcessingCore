
import os
import argparse
import traceback
import inspect

from time import *
from enum import Enum
from datetime import datetime

InParentsDriverContext = 0
InNewDriverContext = 1
InExternalDriverContext = 2

Pending = 0
Positive = 1

class Processing:

	pRoot = None
	rootTickCnt = 0

	def __init__(self):
		self.procDbgLog("Processing()")

		self.parent = None
		self.success = Pending
		self.driverMode = InParentsDriverContext
		self.childList = []
		self.initExecuted = False
		self.finished = False
		self.level = 0
		self.driverContextLevel = 0
		self.exceptionOccured = False
		self.logLevel = 0

	def __del__(self):
		pass

	# This area is used by the client

	def rootTick(self, ptr):

		if Processing.pRoot == None:
			Processing.pRoot = self

		if self.parent == None and self != Processing.pRoot:
			return # this process is not root and has no parent. therefore it hasn't been startet yet

		self.treeTick()

		Processing.rootTickCnt += 1

		if Processing.rootTickCnt > 1:
			sleep(0.002)
			Processing.rootTickCnt = 0

	# This area is used by the concrete processes

	def start(self, pChild, driverMode = InParentsDriverContext):

		if pChild == None:
			self.procDbgLog("pointer to child is zero. not started")
			return None

		procId = pChild.procId()
		self.procDbgLog("starting %s" % procId)

		pChild.success = Pending
		pChild.level = self.level + 1
		pChild.driverContextLevel = self.driverContextLevel
		pChild.driverMode = driverMode
		pChild.parent = self

		self.procDbgLog("adding %s to child list" % procId)
		#{
			#lock_guard<mutex> lock(mChildListMtx)
		self.childList.append(pChild)
		#}
		self.procDbgLog("adding %s to child list: done" % procId)

		self.procDbgLog("starting %s: done" % procId)

		return pChild

	def initialize(self):
		self.procDbgLog("initializing() not used")
		return Positive

	def processInfo(self):
		return ""

	def procId(self):
		iId = id(self)
		return "0x%016X %s %s" % (iId, hash(iId), self.__class__.__name__) # , self.__class__.__module__

	def delProc(self, pChild):

		childId = pChild.procId()

		if pChild.parent != self:
			self.procDbgLog("CRITICAL DESIGN ERROR: process %s is not my child. not deleting it" % childId)
			return

		self.procDbgLog("removing %s from child list" % childId)

		self.childList.remove(pChild)

		while pChild.childList.size():
			pChild.delProc(pChild.childList[0])

		self.procDbgLog("removing %s from child list: done" % childId)

		self.procDbgLog("deleting process %s" % childId)
		pChild = None
		self.procDbgLog("deleting process %s: done" % childId)

	# This area is used by the abstract process

	def treeTick(self):

		for c in self.childList:
			if c.driverMode != InParentsDriverContext:
				continue

			c.treeTick()

		if self.finished:
			return

		if self.exceptionOccured:
			return

		success = Pending

		try:
			if self.initExecuted:
				success = self.process()
			else:
				self.procDbgLog("initializing")
				success = self.initialize()
				self.procDbgLog("initializing: done")
		except KeyboardInterrupt:
			pass
		except:
			traceback.print_exc()
			self.exceptionOccured = True

		if success == Pending:
			return

		if success != Positive or self.initExecuted:
			self.finish(success)
			return

		self.initExecuted = True

	def finish(self, success):
		self.success = success
		self.finished = True
		self.procDbgLog("finished(): success = %d" % success)

	def procErrLog(self, code, message):

		print(message)

		return code

	def procDbgLog(self, message):
		# 201119 22:26:23.263 +  0.000  188   4  start                0xDE8A5B20 094681992682272 MoCmd                       starting 0xDE8A60D0 094681992683728 SystemDebugging

		now = datetime.now()
		ms = now.microsecond / 1000

		logEntry = now.strftime("%d%m%y %H:%M:%S.")
		logEntry += str(int(ms)).rjust(3, "0")

		logEntry += " +  0.000  " # Diff sec

		# logEntry += " " + os.path.basename(inspect.stack()[1][1]) # File
		logEntry += str(inspect.stack()[1][2]).rjust(3) # Linenumber

		logEntry += "   4  " # Debug level

		logEntry += inspect.stack()[1][3].ljust(24) # Function

		logEntry += self.procId().ljust(60)

		logEntry += " " + message

		print(logEntry)

