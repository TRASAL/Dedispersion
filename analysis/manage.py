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

def createTable(queue, TABLE):
  """Create a table to store auto-tuning results for dedispersion."""

  queue.execute("CREATE TABLE " + TABLE + "(id INTEGER NOT NULL PRIMARY KEY AUTO_INCREMENT, DMs INTEGER NOT NULL, samplesPerBlock INTEGER NOT NULL, DMsPerBlock INTEGER NOT NULL, samplesPerThread INTEGER NOT NULL, DMsPerThread INTEGER NOT NULL, GFLOPs FLOAT UNSIGNED NOT NULL, GFLOPs_err FLOAT UNSIGNED NOT NULL, time FLOAT UNSIGNED NOT NULL, time_err FLOAT UNSIGNED NOT NULL)")


def deleteTable(queue, TABLE):
  """Delete table."""

  queue.execute("DROP TABLE " + TABLE)

def loadFile(queue, TABLE, inputFile):
  """Load inputFile into a table in the database."""
  for line in inputFile:
    if ((line[0] != "#") and (line[0] != "\n")) :
      items = line.split(sep = " ")
      queue.execute("INSERT INTO " + TABLE + " VALUES (NULL, " + items[0] + ", " + items[1] + ", " + items[2] + ", " + items[3] + ", " + items[4] + ", " + items[5] + ", " + items[6] + ", " + items[7] + ", " + items[8].rstrip("\n") + ")")
