#!/usr/bin/env python

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
