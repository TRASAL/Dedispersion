#!/usr/bin/env python

def printResults(confs):
  for conf in confs:
    for item in conf:
      print(item, end=" ")
    print()

def getDMRange(queue, TABLE):
  queue.execute("SELECT DISTINCT DMs FROM " + TABLE + " ORDER BY DMs")
  return queue.fetchall()

def export(queue, TABLE, OP):
  confs = list()
  if OP.casefold() == "max" or OP.casefold() == "min":
    DMsRange = getDMRange(queue, TABLE)
    for dm in DMsRange:
      queue.execute("SELECT samplesPerBlock,DMsPerBlock,samplesPerThread,DMsPerThread,GFLOPS,GFLOPS_err,time,time_err FROM " + TABLE + " WHERE (GFLOPS = (SELECT " + OP + "(GFLOPS) FROM " + TABLE + " WHERE (DMs = " + str(dm[0]) + ")) ) AND (DMs = " + str(dm[0]) + ")")
      best = queue.fetchall()
      confs.append([dm[0], best[0][0], best[0][1], best[0][2], best[0][3], best[0][4], best[0][5], best[0][6], best[0][7]])
    return confs

def statistics(queue, TABLE):
  confs = list()
  DMsRange = getDMRange(queue, TABLE)
  for dm in DMsRange:
    queue.execute("SELECT MIN(GFLOPs),AVG(GFLOPs),MAX(GFLOPs),STDDEV_POP(GFLOPs) FROM " + TABLE + " WHERE (DMs = " + str(dm[0]) + ")")
    line = queue.fetchall()
    confs.append([dm[0], line[0][0], line[0][1], line[0][2], line[0][3]])
  return confs

def snr(queue, TABLE):
  confs = list()
  DMsRange = getDMRange(queue, TABLE)
  for dm in DMsRange:
    queue.execute("SELECT MAX(GFLOPs),AVG(GFLOPs),STDDEV_POP(GFLOPs) FROM " + TABLE + " WHERE (DMs = " + str(dm[0]) + ")")
    line = queue.fetchall()[0]
    confs.append([dm[0], (line[0] - line[1]) / line[2]])
  return confs
