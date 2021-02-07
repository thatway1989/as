/**
 * AS - the open source Automotive Software on https://github.com/parai
 *
 * Copyright (C) 2021  AS <parai@foxmail.com>
 *
 * This source code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by the
 * Free Software Foundation; See <http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt>.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */
/* ============================ [ INCLUDES  ] ====================================================== */
#include <pybind11/pybind11.h>
namespace py = pybind11;
/* ============================ [ MACROS    ] ====================================================== */
/* ============================ [ TYPES     ] ====================================================== */
/* ============================ [ DECLARES  ] ====================================================== */
#ifdef USE_LUA_CAN
extern "C" {
int can_open(unsigned long busid,const char* device,unsigned long port, unsigned long baudrate);
int can_write(unsigned long busid,unsigned long canid,unsigned long dlc,unsigned char* data);
int can_read(unsigned long busid,unsigned long canid,unsigned long* p_canid,unsigned long *dlc,unsigned char* data);
int can_close(unsigned long busid);
int can_reset(unsigned long busid);
void luai_canlib_open(void);
void luai_canlib_close(void);
}
#endif
#ifdef USE_LUA_DEV
extern "C" {
	void luai_asdevlib_open(void);
	void luai_asdevlib_close(void);
	int asdev_open(const char* device, const char* option);
	int asdev_write(int fd, unsigned char* data, unsigned long len);
	int asdev_read(int fd, unsigned char** data);
	int asdev_ioctl(int fd, int type, unsigned char* data,unsigned long len, unsigned char** rdata);
	int asdev_close(int fd);
}
#endif
/* ============================ [ DATAS     ] ====================================================== */
/* ============================ [ LOCALS    ] ====================================================== */
/* ============================ [ FUNCTIONS ] ====================================================== */
#ifdef USE_LUA_CAN
class can
{
public:
	explicit can()
	{
		luai_canlib_open();
	}
	~can()
	{
		luai_canlib_close();
	}

	py::object open(unsigned long busid, const char* device,unsigned long port, unsigned long baudrate)
	{
		int r = can_open(busid, device, port, baudrate);
		return py::bool_(r);
	}

	py::object read(unsigned long busid, int canid) {
		unsigned long rcanid,dlc;
		unsigned char data[64];
		int r = can_read(busid,(unsigned long)canid,&rcanid,&dlc,data);
		py::list L;
		L.append(py::bool_(r));
		if(true == r) {
			L.append(py::int_(rcanid));
			L.append(py::int_(dlc));
			L.append(py::bytes((char*)data, dlc));
		} else {
			L.append(py::none());
			L.append(py::none());
			L.append(py::none());
		}
		return L;
	}

	int write(unsigned long busid, unsigned long canid, unsigned long dlc,const char* data) {
		int r = can_write(busid, canid, dlc, (unsigned char*)data);
		return py::bool_(r);
	}

	int close(int busid) {
		int r = can_close(busid);
		return py::bool_(r);
	}

	int reset(unsigned long busid) {
		int r = can_reset(busid);
		return py::bool_(r);
	}
};
#endif
#ifdef USE_LUA_DEV
class asdev
{
public:
	explicit asdev() {
		luai_asdevlib_open();
	}

	~asdev() {
		luai_asdevlib_close();
	}

	py::object open(const char* device, const char* option) {
		int fd = asdev_open(device, option);
		return py::int_(fd);
	}

	py::object write(int fd, const char* data, int len) {
		int r = asdev_write(fd, (unsigned char*)data, (unsigned long)len);
		return py::int_(r);
	}

	py::object read(int fd) {
		unsigned char* data;
		int r = asdev_read(fd, &data);
		py::list L;
		L.append(py::int_(r));
		if(data != NULL) {
			L.append(py::bytes((char*)data, r));
			free(data);
		} else {
			L.append(py::none());
		}
		return L;
	}

	py::object close(int fd) {
		int r = asdev_close(fd);
		return py::int_(r);
	}
};
#endif
PYBIND11_MODULE(AS, m)
{
	m.doc() = "pybind11 AS plugin";
#ifdef USE_LUA_CAN
	py::class_<can>(m, "can")
		.def(py::init<>())
		.def("open", &can::open,
			py::arg("busid"), py::arg("device"), py::arg("port"), py::arg("baudrate"))
		.def("read", &can::read, py::arg("busid"), py::arg("canid"))
		.def("write", &can::write,
			py::arg("busid"), py::arg("canid"), py::arg("dlc"), py::arg("data"))
		.def("close", &can::close, py::arg("busid"))
		.def("reset", &can::reset, py::arg("busid"));
#endif
#ifdef USE_LUA_DEV
	py::class_<asdev>(m, "asdev")
		.def(py::init<>())
		.def("open", &asdev::open,
			py::arg("device"), py::arg("option"))
		.def("write", &asdev::write,
			py::arg("fd"), py::arg("data"), py::arg("len"))
		.def("read", &asdev::read, py::arg("fd"))
		.def("close", &asdev::close, py::arg("fd"));
#endif
}
