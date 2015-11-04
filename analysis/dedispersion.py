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
import analysis
import speedup

if len(sys.argv) == 1:
    print("Supported commands are: create, list, delete, load, tune, tuneNoReuse, statistics, histogram, optimizationSpace, singleParameterOptimizationSpace, speedup, speedupNoReuse")
    sys.exit(1)

COMMAND = sys.argv[1]

DB_CONN = pymysql.connect(host=config.HOST, port=config.PORT, user=config.USER, passwd=config.PASS, db=config.DEDISPERSION_DB)
QUEUE = DB_CONN.cursor()

if COMMAND == "create":
    if len(sys.argv) != 3:
        print("Usage: " + sys.argv[0] + " create <table>")
        QUEUE.close()
        DB_CONN.close()
        sys.exit(1)
    try:
        manage.create_table(QUEUE, sys.argv[2])
    except pymysql.err.InternalError:
        pass
elif COMMAND == "list":
    if len(sys.argv) != 2:
        print("Usage: " + sys.argv[0] + " list")
        QUEUE.close()
        DB_CONN.close()
        sys.exit(1)
    try:
        manage.print_results(manage.get_tables(QUEUE))
    except pymysql.err.InternalError:
        pass
elif COMMAND == "delete":
    if len(sys.argv) != 3:
        print("Usage: " + sys.argv[0] + " delete <table>")
        QUEUE.close()
        DB_CONN.close()
        sys.exit(1)
    try:
        manage.delete_table(QUEUE, sys.argv[2])
    except pymysql.err.InternalError:
        pass
elif COMMAND == "load":
    if len(sys.argv) != 4:
        print("Usage: " + sys.argv[0] + " load <table> <input_file>")
        QUEUE.close()
        DB_CONN.close()
        sys.exit(1)
    INPUT_FILE = open(sys.argv[3])
    try:
        manage.load_file(QUEUE, sys.argv[2], INPUT_FILE)
    except:
        print(sys.exc_info())
elif COMMAND == "tune":
    if len(sys.argv) < 6 or len(sys.argv) > 8:
        print("Usage: " + sys.argv[0] + " tune <table> <operator> <channels> <samples> [local|cache] [split|cont]")
        QUEUE.close()
        DB_CONN.close()
        sys.exit(1)
    try:
        FLAGS = [0, 0]
        if "local" in sys.argv:
            FLAGS[0] = 1
        elif "cache" in sys.argv:
            FLAGS[0] = 2
        if "split" in sys.argv:
            FLAGS[1] = 1
        elif "cont" in sys.argv:
            FLAGS[1] = 2
        CONFS = export.tune(QUEUE, sys.argv[2], sys.argv[3], sys.argv[4], sys.argv[5], FLAGS)
        manage.print_results(CONFS)
    except:
        print(sys.exc_info())
elif COMMAND == "tuneNoReuse":
    if len(sys.argv) < 6 or len(sys.argv) > 8:
        print("Usage: " + sys.argv[0] + " tuneNoReuse <table> <operator> <channels> <samples> [local|cache] [split|cont]")
        QUEUE.close()
        DB_CONN.close()
        sys.exit(1)
    try:
        FLAGS = [0, 0]
        if "local" in sys.argv:
            FLAGS[0] = 1
        elif "cache" in sys.argv:
            FLAGS[0] = 2
        if "split" in sys.argv:
            FLAGS[1] = 1
        elif "cont" in sys.argv:
            FLAGS[1] = 2
        CONFS = export.tune_no_reuse(QUEUE, sys.argv[2], sys.argv[3], sys.argv[4], sys.argv[5], FLAGS)
        manage.print_results(CONFS)
    except:
        print(sys.exc_info())
elif COMMAND == "statistics":
    if len(sys.argv) < 5 or len(sys.argv) > 6:
        print("Usage: " + sys.argv[0] + " statistics <table> <channels> <samples> [local|cache]")
        QUEUE.close()
        DB_CONN.close()
        sys.exit(1)
    try:
        FLAGS = [False, False]
        if "local" in sys.argv:
            FLAGS[0] = True
        elif "cache" in sys.argv:
            FLAGS[1] = False
        CONFS = analysis.statistics(QUEUE, sys.argv[2], sys.argv[3], sys.argv[4], FLAGS)
        manage.print_results(CONFS)
    except:
        print(sys.exc_info())
elif COMMAND == "histogram":
    if len(sys.argv) < 5 or len(sys.argv) > 6:
        print("Usage: " + sys.argv[0] + " histogram <table> <channels> <samples> [local|cache]")
        QUEUE.close()
        DB_CONN.close()
        sys.exit(1)
    try:
        FLAGS = [False, False]
        if "local" in sys.argv:
            FLAGS[0] = True
        elif "cache" in sys.argv:
            FLAGS[1] = False
        HISTS = analysis.histogram(QUEUE, sys.argv[2], sys.argv[3], sys.argv[4], FLAGS)
        for hist in HISTS:
            i = 0
            for item in hist:
                print(i, item)
                i = i + 1
            print("\n\n")
    except:
        print(sys.exc_info())
elif COMMAND == "optimizationSpace":
    if len(sys.argv) < 5 or len(sys.argv) > 6:
        print("Usage: " + sys.argv[0] + " optimizationSpace <table> <channels> <samples> [local|cache]")
        QUEUE.close()
        DB_CONN.close()
        sys.exit(1)
    try:
        FLAGS = [False, False]
        if "local" in sys.argv:
            FLAGS[0] = True
        elif "cache" in sys.argv:
            FLAGS[1] = True
        CONFS = analysis.optimization_space(QUEUE, sys.argv[2], sys.argv[3], sys.argv[4], FLAGS)
        manage.print_results(CONFS)
    except:
        print(sys.exc_info())
elif COMMAND == "singleParameterOptimizationSpace":
    if len(sys.argv) < 6 or len(sys.argv) > 7:
        print("Usage: " + sys.argv[0] + " singleParameterOptimizationSpace <table> <parameter> <channels> <samples> [local|cache]")
        QUEUE.close()
        DB_CONN.close()
        sys.exit(1)
    try:
        FLAGS = [False, False]
        if "local" in sys.argv:
            FLAGS[0] = True
        elif "cache" in sys.argv:
            FLAGS[1] = True
        CONFS = analysis.single_parameter_space(QUEUE, sys.argv[2], sys.argv[3], sys.argv[4], sys.argv[5], FLAGS)
        for row in CONFS:
            for item in row:
                print(item[0], item[1])
            print("\n")
    except:
        print(sys.exc_info())
elif COMMAND == "speedup":
    if len(sys.argv) != 6:
        print("Usage: " + sys.argv[0] + " speedup <table> <reference_table> <channels> <samples>")
        QUEUE.close()
        DB_CONN.close()
        sys.exit(1)
    try:
        CONFS = speedup.speedup(QUEUE, sys.argv[2], sys.argv[3], sys.argv[4], sys.argv[5])
        manage.print_results(CONFS)
    except:
        print(sys.exc_info())
elif COMMAND == "speedupNoReuse":
    if len(sys.argv) != 5:
        print("Usage: " + sys.argv[0] + " speedupNoReuse <table> <channels> <samples>")
        QUEUE.close()
        DB_CONN.close()
        sys.exit(1)
    try:
        CONFS = speedup.speedupNoReuse(QUEUE, sys.argv[2], sys.argv[3], sys.argv[4])
        manage.print_results(CONFS)
    except:
        print(sys.exc_info())
else:
    print("Unknown command.")
    print("Supported commands are: create, list, delete, load, tune, tuneNoReuse, statistics, histogram, optimizationSpace, singleParameterOptimizationSpace, speedup, speedupNoReuse")

QUEUE.close()
DB_CONN.commit()
DB_CONN.close()
sys.exit(0)

