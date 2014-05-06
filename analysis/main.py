#!/usr/bin/env python
# Copyright 2014 Alessio Sclocco <a.sclocco@vu.nl>
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import sys
import pymysql
import config
import manage
import export

COMMAND = sys.argv[1]

dbConn = pymysql.connect(host=config.myHost, port=config.myPort, user=config.myUser, passwd=config.myPass, db=config.myDb)
queue = dbConn.cursor()

if COMMAND == "create":
  try:
    manage.createTable(queue, sys.argv[2])
  except pymysql.err.InternalError:
    pass
elif COMMAND == "delete":
  try:
    manage.deleteTable(queue, sys.argv[2])
  except pymysql.err.InternalError:
    pass
elif COMMAND == "load":
  inputFile = open(sys.argv[3])
  try:
    manage.loadFile(queue, sys.argv[2], inputFile)
  except:
    print(sys.exc_info()[0])
    sys.exit(1)
elif COMMAND == "export":
  try:
    confs = export.export(queue, sys.argv[2], sys.argv[3])
    export.printResults(confs)
  except pymysql.err.ProgrammingError:
    pass
  except:
    print(sys.exc_info()[0])
    sys.exit(1)
elif COMMAND == "tuneNoReuse":
  try:
    confs = export.tuneNoReuse(queue, sys.argv[2], sys.argv[3])
    export.printResults(confs)
  except pymysql.err.ProgrammingError:
    pass
  except:
    print(sys.exc_info()[0])
    sys.exit(1)
elif COMMAND == "tuneThreadsNoReuse":
  try:
    confs = export.tuneThreadsNoReuse(queue, sys.argv[2], sys.argv[3])
    export.printResults(confs)
  except pymysql.err.ProgrammingError:
    pass
  except:
    print(sys.exc_info()[0])
    sys.exit(1)
elif COMMAND == "statistics":
  try:
    confs = export.statistics(queue, sys.argv[2])
    export.printResults(confs)
  except pymysql.err.ProgrammingError:
    pass
  except:
    print(sys.exc_info()[0])
    sys.exit(1)
elif COMMAND == "snr":
  try:
    confs = export.snr(queue, sys.argv[2])
    export.printResults(confs)
  except pymysql.err.ProgrammingError:
    pass
  except:
    print(sys.exc_info()[0])
    sys.exit(1)
else:
  print("Unknown command.")
  print("Supported commands are: create, delete, load, export, tuneNoReuse, tuneThreadsNoReuse, statistics, snr")

queue.close()
dbConn.commit()
dbConn.close()
sys.exit(0)
