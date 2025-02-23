/*
 * $Id: _psutil_bsd.h 1142 2011-10-05 18:45:49Z g.rodola $
 *
 * Copyright (c) 2009, Jay Loden, Giampaolo Rodola'. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * BSD platform-specific module methods for _psutil_bsd
 */

#include <Python.h>

// --- per-process functions

static PyObject* get_cpu_times(PyObject* self, PyObject* args);
static PyObject* get_process_name(PyObject* self, PyObject* args);
static PyObject* get_process_exe(PyObject* self, PyObject* args);
static PyObject* get_process_cmdline(PyObject* self, PyObject* args);
static PyObject* get_process_ppid(PyObject* self, PyObject* args);
static PyObject* get_process_uids(PyObject* self, PyObject* args);
static PyObject* get_process_gids(PyObject* self, PyObject* args);
static PyObject* get_process_create_time(PyObject* self, PyObject* args);
static PyObject* get_memory_info(PyObject* self, PyObject* args);
static PyObject* get_process_num_threads(PyObject* self, PyObject* args);
static PyObject* get_process_threads(PyObject* self, PyObject* args);
static PyObject* get_process_status(PyObject* self, PyObject* args);
static PyObject* get_process_io_counters(PyObject* self, PyObject* args);
static PyObject* get_process_tty_nr(PyObject* self, PyObject* args);

// --- system-related functions

static PyObject* get_pid_list(PyObject* self, PyObject* args);
static PyObject* get_num_cpus(PyObject* self, PyObject* args);
static PyObject* get_total_phymem(PyObject* self, PyObject* args);
static PyObject* get_avail_phymem(PyObject* self, PyObject* args);
static PyObject* get_total_virtmem(PyObject* self, PyObject* args);
static PyObject* get_avail_virtmem(PyObject* self, PyObject* args);
static PyObject* get_system_cpu_times(PyObject* self, PyObject* args);
static PyObject* get_system_per_cpu_times(PyObject* self, PyObject* args);
static PyObject* get_system_boot_time(PyObject* self, PyObject* args);
static PyObject* get_disk_partitions(PyObject* self, PyObject* args);
static PyObject* get_network_io_counters(PyObject* self, PyObject* args);
