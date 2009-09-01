#include <Python.h>
#include "wpsapi.h"
#include "structmember.h"

static PyObject *PyhookError;
static WPS_SimpleAuthentication auth;

static char *
get_error_string(WPS_ReturnCode rc) {
  if (rc == WPS_OK) {
    return "A-Okay, captain!";
  }
  else if (rc == WPS_ERROR_SCANNER_NOT_FOUND) {
    return "The WPSScanner.dll was not found.";
  }
  else if (rc == WPS_ERROR_WIFI_NOT_AVAILABLE) {
    return "No Wifi adapter was detected.";
  }
  else if (rc == WPS_ERROR_NO_WIFI_IN_RANGE) {
    return "No Wifi reference points in range.";
  }
  else if (rc == WPS_ERROR_UNAUTHORIZED) {
    return "User authentication failed.";
  }
  else if (rc == WPS_ERROR_SERVER_UNAVAILABLE) {
    return "The server is unavailable.";
  }
  else if (rc == WPS_ERROR_LOCATION_CANNOT_BE_DETERMINED) {
    return "A location couldn't be determined.";
  }
  else if (rc == WPS_ERROR_PROXY_UNAUTHORIZED) {
    return "Proxy authentication failed.";
  }
  else if (rc == WPS_ERROR_FILE_IO) {
    return "A file IO error occurred while reading the local file.";
  }
  else if (rc == WPS_ERROR_INVALID_FILE_FORMAT) {
    return "The local file has an invalid format.";
  }
  else if (rc == WPS_ERROR_TIMEOUT) {
    return "Network operation timed out.";
  }
  else if (rc == WPS_NOMEM) {
    return "Cannot allocate memory.";
  }
  else {
    return "Unknown error occurred.";
  }
}

typedef struct {
  PyObject_HEAD
  int NONE;
  int LIMITED;
  int FULL;
} pyhook_StreetAddressLookup;

static PyTypeObject pyhook_StreetAddressLookupType = {
  PyObject_HEAD_INIT(NULL)
    0,
    "pyhook.StreetAddressLookup",
    sizeof(pyhook_StreetAddressLookup),
    0,                         /*tp_itemsize*/
    0,                         /*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    0,                         /*tp_compare*/
    0,                         /*tp_repr*/
    0,                         /*tp_as_number*/
    0,                         /*tp_as_sequence*/
    0,                         /*tp_as_mapping*/
    0,                         /*tp_hash */
    0,                         /*tp_call*/
    0,                         /*tp_str*/
    0,                         /*tp_getattro*/
    0,                         /*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT,        /*tp_flags*/
    "Street address lookup type option",           /* tp_doc */
};

static PyMemberDef StreetAddressLookup_members[] = {
  {"NONE", T_INT, offsetof(pyhook_StreetAddressLookup, NONE), 0, "No street address lookup is performed."},
  {"LIMITED", T_INT, offsetof(pyhook_StreetAddressLookup, LIMITED), 0, "A limited address lookup is performed to return, at most, city information."},
  {"FULL", T_INT, offsetof(pyhook_StreetAddressLookup, FULL), 0, "A full street address lookup is performed returning the most specific street address."},
  {NULL} /* Sentinel */
};

static PyObject *
StreetAddressLookup_new(PyTypeObject *type, PyObject *args, PyObject *kwds) {
  pyhook_StreetAddressLookup *self;
  
  self = (pyhook_StreetAddressLookup *)type->tp_alloc(type, 0);
  if (self != NULL) {
    self->NONE = WPS_NO_STREET_ADDRESS_LOOKUP;
    self->LIMITED = WPS_LIMITED_STREET_ADDRESS_LOOKUP;
    self->FULL = WPS_FULL_STREET_ADDRESS_LOOKUP;
  }
  return (PyObject *)self;
}

static PyObject* convertStreetAddress(void *address) {
  WPS_StreetAddress *addy = (WPS_StreetAddress *)address;
  PyObject *object;
  if (addy == NULL) {
    Py_INCREF(Py_None);
    object = Py_None;
    return object;
  }
  printf("%s", addy->street_number);

  PyObject *addy_lines = PyList_New(0);
  if (addy_lines == NULL)
    return 0;

  char **line;
  for (line = addy->address_line; *line; ++line) {
    if (PyList_Append(addy_lines, Py_BuildValue("s", *line)) != 0) {
      Py_DECREF(addy_lines);
      return NULL;
    }
  }

  
  object = Py_BuildValue("{s:s, s:s, s:s, s:s, s:s, s:s, s:N}",
			 "streetNumber", addy->street_number,
			 "city", addy->city,
			 "postalCode", addy->postal_code,
			 "county", addy->county,
			 "province", addy->province,
			 "region", addy->region,
  			 "addressLines", addy_lines);

  return object;
}

static PyObject *
pyhook_BuildLocationValue(const WPS_Location *location) {
  PyObject *retval;
  printf("DEBUG: in build location value\n");
  printf("DEBUG: latitude = %f\n", location->latitude);
  retval = Py_BuildValue("{s:d, s:H, s:d, s:d, s:d, s:d, s:O&}",
			 "hpe", location->hpe,
			 "nap", location->nap,
			 "speed", location->speed,
			 "bearing", location->bearing,
			 "latitude", location->latitude,
			 "longitude", location->longitude,
			 "streetAddress", convertStreetAddress, location->street_address);
  return retval;
}

static PyObject *
pyhook_init(PyObject *self, PyObject *args, PyObject *keywords) {
  
  char *realm = "ccsolution";
  char *username = "david";
  
  static char *kwlist[] = {"realm", "username", NULL};
  if (!PyArg_ParseTupleAndKeywords(args, 
				   keywords, 
				   "ss", 
				   kwlist, 
				   &realm, &username))
    return NULL;

  auth.username = username;
  auth.realm = realm;
  
  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject *
pyhook_iplocation(PyObject *self, PyObject *args) {
  WPS_IPLocation *ip_location;
  WPS_ReturnCode rc;
  PyObject *retval;
  int addressLookupType = WPS_NO_STREET_ADDRESS_LOOKUP;
  
  if (!PyArg_ParseTuple(args, "|i", &addressLookupType))
    return NULL;
  
  rc = WPS_ip_location(&auth,
		       addressLookupType,
		       &ip_location);
  if (rc != WPS_OK) {
    PyErr_SetString(PyhookError, get_error_string(rc));
    retval = NULL;
  }
  else {
    retval = Py_BuildValue("{s:s,s:f,s:f,s:O&}",
			   "ip", ip_location->ip,
			   "latitude", ip_location->latitude,
			   "longitude", ip_location->longitude,
			   "streetAddress", convertStreetAddress, ip_location->street_address);
    
    WPS_free_ip_location(ip_location);
  }
  
  return retval;
}

PyObject *
pyhook_location(PyObject *self, PyObject *args) {
  WPS_Location *location;
  WPS_ReturnCode rc;
  PyObject *retval;
  int addressLookupType = WPS_NO_STREET_ADDRESS_LOOKUP;
  
  if (!PyArg_ParseTuple(args, "|i", &addressLookupType))
    return NULL;
  
  rc = WPS_location(&auth,
		    addressLookupType,
		    &location);
  
  if (rc != WPS_OK) {
    PyErr_SetString(PyhookError, get_error_string(rc));
    retval = NULL;
  }
  else {
    retval = pyhook_BuildLocationValue(location);
    WPS_free_location(location);
  }

  return retval;
}

typedef struct {
  PyObject *data;
  PyObject *callback;
} pyhook_PeriodicArg;

WPS_Continuation pyhook_periodicCallback(void *arg, WPS_ReturnCode code, const WPS_Location *location) {
  pyhook_PeriodicArg *periodicArg = (pyhook_PeriodicArg *)arg;
  PyObject *result;
  PyObject *arglist;
  PyObject *pyLocation;
  WPS_Continuation doContinue = WPS_CONTINUE;
  
  if (location != NULL) {
    pyLocation = pyhook_BuildLocationValue(location);
  
    if(pyLocation == NULL) {
      doContinue = WPS_STOP;
    }
  } else {
    Py_INCREF(Py_None);
    pyLocation = Py_None;
  }
  
  if (doContinue == WPS_CONTINUE) {
    printf("DEBUG: building value, data = %x\n", (unsigned int)periodicArg->data);
    printf("DEBUG: pyLocation = %x\n", (unsigned int)pyLocation);
    arglist = Py_BuildValue("(NiN)", periodicArg->data, (int)code, pyLocation);
    if(arglist == NULL) {
      Py_DECREF(pyLocation);
      doContinue = WPS_STOP;
    }
  }
  
  if (doContinue == WPS_CONTINUE) {
    printf("DEBUG: doing call\n");
    result = PyObject_CallObject(periodicArg->callback, arglist);
    Py_DECREF(arglist);
    Py_DECREF(pyLocation);
    
    if(result == NULL) {
      doContinue = WPS_STOP;
    }
    Py_DECREF(result);
  }
  
  if (doContinue == WPS_STOP) {
    Py_DECREF(periodicArg->callback);
    Py_XDECREF(periodicArg->data);
  }

  return doContinue;
}  

PyObject *
pyhook_periodicLocation(PyObject *self, PyObject *args) {
  WPS_ReturnCode rc;
  int addressLookupType;
  unsigned long period;
  unsigned iterations;
  pyhook_PeriodicArg arg;
  
  if(!PyArg_ParseTuple(args, "kIOO|i", &period, &iterations,
		       &arg.callback, &arg.data, &addressLookupType))
    return NULL;

  if(period == 0) {
    PyErr_SetString(PyhookError, "Period must be strictly greater than 0.");
    return NULL;
  }

  Py_XINCREF(arg.callback);
  if (!PyCallable_Check(arg.callback)) {
    PyErr_SetString(PyExc_TypeError, "parameter must be callable");
    Py_XDECREF(arg.callback);
    return NULL;
  }
  
  Py_XINCREF(arg.data);
  
  rc = WPS_periodic_location(&auth,
			     addressLookupType,
			     period,
			     iterations,
			     pyhook_periodicCallback,
			     (void *)&arg);

  if(rc != WPS_OK) {
    PyErr_SetString(PyhookError, get_error_string(rc));
    Py_DECREF(arg.callback);
    Py_XDECREF(arg.data);
    return NULL;
  }
  
  Py_INCREF(Py_None);
  return Py_None;
}

static PyMethodDef PyhookMethods[] = {
  {"ipLocation", pyhook_iplocation, METH_VARARGS, "Look up location based on IP address."},
    {"init", (PyCFunction)pyhook_init, METH_VARARGS | METH_KEYWORDS, "Initialize the module with authentication credentials."},
  {"location", pyhook_location, METH_VARARGS, "Requests geographic location based on observed wifi access points, cell towers, and GPS signals."},
  {"periodicLocation", pyhook_periodicLocation, METH_VARARGS, "Requests periodic geographic location based on observed wifi access points, cell towers, and GPS signals."},
  {NULL, NULL, 0, NULL}
};

PyMODINIT_FUNC
initpyhook(void) {
  PyObject *m;
  
  pyhook_StreetAddressLookupType.tp_members = StreetAddressLookup_members;
  if (PyType_Ready(&pyhook_StreetAddressLookupType) < 0)
    return;
  
  m = Py_InitModule("pyhook", PyhookMethods);
  if (m == NULL)
    return;

  PyhookError = PyErr_NewException("pyhook.error", NULL, NULL);
  Py_INCREF(PyhookError);
  PyModule_AddObject(m, "error", PyhookError);

  Py_INCREF(&pyhook_StreetAddressLookupType);
  PyModule_AddObject(m, "StreetAddressLookup", StreetAddressLookup_new(&pyhook_StreetAddressLookupType, NULL, NULL));
}
