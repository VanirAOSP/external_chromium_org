# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for the AdbWrapper class."""

import os
import socket
import tempfile
import time
import unittest

import adb_wrapper


class TestAdbWrapper(unittest.TestCase):

  def setUp(self):
    devices = adb_wrapper.AdbWrapper.GetDevices()
    assert devices, 'A device must be attached'
    self._adb = devices[0]
    self._adb.WaitForDevice()

  def _MakeTempFile(self, contents):
    """Make a temporary file with the given contents.
    
    Args:
      contents: string to write to the temporary file.

    Returns:
      The absolute path to the file.
    """
    fi, path = tempfile.mkstemp()
    with os.fdopen(fi, 'wb') as f:
      f.write('foo')
    return path

  def testShell(self):
    output = self._adb.Shell('echo test', expect_rc=0)
    self.assertEqual(output.strip(), 'test')
    output = self._adb.Shell('echo test')
    self.assertEqual(output.strip(), 'test')
    self.assertRaises(adb_wrapper.CommandFailedError, self._adb.Shell,
        'echo test', expect_rc=1)

  def testPushPull(self):
    path = self._MakeTempFile('foo')
    device_path = '/data/local/tmp/testfile.txt'
    local_tmpdir = os.path.dirname(path)
    self._adb.Push(path, device_path)
    self.assertEqual(self._adb.Shell('cat %s' % device_path), 'foo')
    self._adb.Pull(device_path, local_tmpdir)
    with open(os.path.join(local_tmpdir, 'testfile.txt'), 'r') as f:
      self.assertEqual(f.read(), 'foo')

  def testInstall(self):
    path = self._MakeTempFile('foo')
    self.assertRaises(adb_wrapper.CommandFailedError, self._adb.Install, path)

  def testForward(self):
    self.assertRaises(adb_wrapper.CommandFailedError, self._adb.Forward, 0, 0)

  def testUninstall(self):
    self.assertRaises(adb_wrapper.CommandFailedError, self._adb.Uninstall,
        'some.nonexistant.package')

  def testRebootWaitForDevice(self):
    self._adb.Reboot()
    print 'waiting for device to reboot...'
    while self._adb.GetState() == 'device':
      time.sleep(1)
    self._adb.WaitForDevice()
    self.assertEqual(self._adb.GetState(), 'device')
    print 'waiting for package manager...'
    while 'package:' not in self._adb.Shell('pm path android'):
      time.sleep(1)

  def testRootRemount(self):
    self._adb.Root()
    while True:
      try:
        self._adb.Shell('start')
        break
      except adb_wrapper.CommandFailedError:
        time.sleep(1)
    self._adb.Remount()


if __name__ == '__main__':
  unittest.main()
