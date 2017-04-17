#     Copyright 2015 Cedraro Andrea <a.cedraro@gmail.com>
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
#    limitations under the License.


from jedihttp import utils
utils.AddVendorFolderToSysPath()
import codecs

import contextlib
import logging
import json
import bottle
from bottle import response, request, Bottle
from jedihttp import hmaclib
from jedihttp.compatibility import iteritems
from threading import Lock

try:
  import httplib
except ImportError:
  from http import client as httplib



import build.swiftvi as swiftvi

# Wrap a completion
class YCMCompletion():
    """
    {
      "key.kind": "source.lang.swift.decl.function.method.instance",
      "key.name": "`self`() -> Self",
      "key.sourcetext": "`self`() -> Self {\n<#code#>\n}",
      "key.description": "`self`() -> Self",
      "key.typename": "",
      "key.context": "source.codecompletion.context.superclass",
      "key.num_bytes_to_erase": 0,
      "key.substructure": {
        "key.nameoffset": 0,
        "key.namelength": 16
      },
      "key.associated_usrs": "c:objc(pl)NSObject(im)self",
      "key.modulename": "ObjectiveC.NSObject"
    }
    """
    def docstring(self):
        if self.docbrief:
            return self.description + "\n" + self.docbrief
        return self.description + "\n" +  self.context

    def __init__( self, jsonValue ):
        self.module_path = ""
        self.name = jsonValue.get("key.sourcetext")
        self.description  = jsonValue.get("key.description")
        self.type = "swift"
        self.line = 0
        self.column = 0
        self.modulename = jsonValue.get("key.modulename")
        self.context = jsonValue.get("key.context")
        self.docbrief = jsonValue.get("key.doc.brief")

class YCMCompletionDoc():
    def completions(self):
        completions = []
        for jsonCompletion in self.JSONString.get("key.results"):
            completions.append(YCMCompletion(jsonCompletion))
        return completions

    def usages():
        return self.JSONString

    def __init__( self, value ):
        self.JSONString = json.loads(value)

def run(source_path, current_content, line, col):
    # TODO: Allow selection of flags
    flags = swiftvi.StringList()
    flags.append("-sdk")
    flags.append("/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk")
    flags.append("-target")
    flags.append("x86_64-apple-macosx10.12")

    runner = swiftvi.Runner()
    fileName = source_path.encode('utf-8')
    content = current_content.encode('utf-8')
    result = runner.complete(fileName, content, flags, col, line)
    return YCMCompletionDoc(result)


def Sema_preload_module(modules):
    return run(15, 19)

Sema_settings = {}

def Sema_Script(source,
                      line,
                      col,
                      source_path):
    return run(source_path, source, line, col)

def Sema_names(source,
                     path,
                     all_scopes,
                     definitions,
                     references):
    return run(19, 15)


# num bytes for the request body buffer; request.json only works if the request
# size is less than this
bottle.Request.MEMFILE_MAX = 1000 * 1024

logger = logging.getLogger( __name__ )
logger.debug
app = Bottle( __name__ )

# Jedi is not thread safe.
jedi_lock = Lock()

# For efficiency, we store the default values of the global Jedi settings. ##
# TODO?
default_settings = {
    'case_insensitive_completion'     : "",
}

@app.post( '/healthy' )
def healthy():
  logger.debug( 'received /healthy request' )
  return _JsonResponse( True )


@app.post( '/ready' )
def ready():
  logger.debug( 'received /ready request' )
  return _JsonResponse( True )


@app.post( '/completions' )
def completions():
  logger.debug( 'received /completions request' )
  with jedi_lock:
    request_json = request.json
    with _CustomSettings( request_json ):
      script = _GetSemaDoc( request_json )
      response = _FormatCompletions( script.completions() )
  return _JsonResponse( response )


@app.post( '/gotodefinition' )
def gotodefinition():
  logger.debug( 'received /gotodefinition request' )
  with jedi_lock:
    request_json = request.json
    with _CustomSettings( request_json ):
      script = _GetSemaDoc( request_json )
      response = _FormatDefinitions( script.goto_definitions() )
  return _JsonResponse( response )


@app.post( '/gotoassignment' )
def gotoassignments():
  logger.debug( 'received /gotoassignment request' )
  with jedi_lock:
    request_json = request.json
    follow_imports = request_json.get( 'follow_imports', False )
    with _CustomSettings( request_json ):
      script = _GetSemaDoc( request_json )
      response = _FormatDefinitions( script.goto_assignments( follow_imports ) )
  return _JsonResponse( response )


@app.post( '/usages' )
def usages():
  logger.debug( 'received /usages request' )
  with jedi_lock:
    request_json = request.json
    with _CustomSettings( request_json ):
      script = _GetSemaDoc( request_json )
      response = _FormatDefinitions( script.usages() )
  return _JsonResponse( response )


@app.post( '/names' )
def names():
  logger.debug( 'received /names request' )
  with jedi_lock:
    request_json = request.json
    with _CustomSettings( request_json ):
      definitions = _GetSemaNames( request_json )
      response = _FormatDefinitions( definitions )
  return _JsonResponse( response )


@app.post( '/preload_module' )
def preload_module():
  logger.debug( 'received /preload_module request' )
  with jedi_lock:
    request_json = request.json
    with _CustomSettings( request_json ):
      Sema_preload_module( *request_json[ 'modules' ] )
  return _JsonResponse( True )


def _FormatCompletions( completions ):
  return {
      'completions': [ {
          'module_path': completion.module_path,
          'name':        completion.name,
          'type':        completion.type,
          'line':        completion.line,
          'column':      completion.column,
          'docstring':   completion.docstring(),
          'description': completion.description,
      } for completion in completions ]
  }


def _FormatDefinitions( definitions ):
  return {
      'definitions': [ {
          'module_path':       definition.module_path,
          'name':              definition.name,
          'type':              definition.type,
          'in_builtin_module': definition.in_builtin_module(),
          'line':              definition.line,
          'column':            definition.column,
          'docstring':         definition.docstring(),
          'description':       definition.description,
          'full_name':         definition.full_name,
          'is_keyword':        definition.is_keyword
      } for definition in definitions ]
  }


def _GetSemaDoc( request_data ):
  return Sema_Script( request_data[ 'source' ],
                      request_data[ 'line' ],
                      request_data[ 'col' ],
                      request_data[ 'source_path' ] )


def _GetSemaNames( request_data ):
  return Sema_names( source = request_data[ 'source' ],
                     path = request_data[ 'path' ],
                     all_scopes = request_data.get( 'all_scopes', False ),
                     definitions = request_data.get( 'definitions', True ),
                     references = request_data.get( 'references', False ) )


def _SetJediSettings( settings ):
  for name, value in iteritems( settings ):
    setattr( Sema_settings, name, value )

@contextlib.contextmanager
def _CustomSettings( request_data ):
  settings = request_data.get( 'settings' )
  if not settings:
    yield
    return
  try:
    _SetJediSettings( settings )
    yield
  finally:
    _SetJediSettings( default_settings )


@app.error( httplib.INTERNAL_SERVER_ERROR )
def ErrorHandler( httperror ):
  body = _JsonResponse( {
    'exception': httperror.exception,
    'message': str( httperror.exception ),
    'traceback': httperror.traceback
  } )
  if 'jedihttp.hmac_secret' in app.config:
    hmac_secret = app.config[ 'jedihttp.hmac_secret' ]
    hmachelper = hmaclib.JediHTTPHmacHelper( hmac_secret )
    hmachelper.SignResponseHeaders( response.headers, body )
  return body


def _JsonResponse( data ):
  response.content_type = 'application/json'
  return json.dumps( data, default = _Serializer )


def _Serializer( obj ):
  try:
    serialized = obj.__dict__.copy()
    serialized[ 'TYPE' ] = type( obj ).__name__
    return serialized
  except AttributeError:
    return str( obj )
