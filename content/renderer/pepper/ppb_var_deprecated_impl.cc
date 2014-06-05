// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/ppb_var_deprecated_impl.h"

#include <limits>

#include "content/renderer/pepper/common.h"
#include "content/renderer/pepper/host_globals.h"
#include "content/renderer/pepper/npapi_glue.h"
#include "content/renderer/pepper/npobject_var.h"
#include "content/renderer/pepper/pepper_plugin_instance_impl.h"
#include "content/renderer/pepper/plugin_module.h"
#include "content/renderer/pepper/plugin_object.h"
#include "ppapi/c/dev/ppb_var_deprecated.h"
#include "ppapi/c/ppb_var.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/shared_impl/ppb_var_shared.h"
#include "third_party/WebKit/public/web/WebBindings.h"
#include "third_party/WebKit/public/web/WebScopedUserGesture.h"

using ppapi::NPObjectVar;
using ppapi::PpapiGlobals;
using ppapi::StringVar;
using ppapi::Var;
using blink::WebBindings;

namespace content {

namespace {

const char kInvalidObjectException[] = "Error: Invalid object";
const char kInvalidPropertyException[] = "Error: Invalid property";
const char kInvalidValueException[] = "Error: Invalid value";
const char kUnableToGetPropertyException[] = "Error: Unable to get property";
const char kUnableToSetPropertyException[] = "Error: Unable to set property";
const char kUnableToRemovePropertyException[] =
    "Error: Unable to remove property";
const char kUnableToGetAllPropertiesException[] =
    "Error: Unable to get all properties";
const char kUnableToCallMethodException[] = "Error: Unable to call method";
const char kUnableToConstructException[] = "Error: Unable to construct";

// ---------------------------------------------------------------------------
// Utilities

// Converts the given PP_Var to an NPVariant, returning true on success.
// False means that the given variant is invalid. In this case, the result
// NPVariant will be set to a void one.
//
// The contents of the PP_Var will NOT be copied, so you need to ensure that
// the PP_Var remains valid while the resultant NPVariant is in use.
bool PPVarToNPVariantNoCopy(PP_Var var, NPVariant* result) {
  switch (var.type) {
    case PP_VARTYPE_UNDEFINED:
      VOID_TO_NPVARIANT(*result);
      break;
    case PP_VARTYPE_NULL:
      NULL_TO_NPVARIANT(*result);
      break;
    case PP_VARTYPE_BOOL:
      BOOLEAN_TO_NPVARIANT(var.value.as_bool, *result);
      break;
    case PP_VARTYPE_INT32:
      INT32_TO_NPVARIANT(var.value.as_int, *result);
      break;
    case PP_VARTYPE_DOUBLE:
      DOUBLE_TO_NPVARIANT(var.value.as_double, *result);
      break;
    case PP_VARTYPE_STRING: {
      StringVar* string = StringVar::FromPPVar(var);
      if (!string) {
        VOID_TO_NPVARIANT(*result);
        return false;
      }
      const std::string& value = string->value();
      STRINGN_TO_NPVARIANT(value.c_str(), value.size(), *result);
      break;
    }
    case PP_VARTYPE_OBJECT: {
      scoped_refptr<NPObjectVar> object(NPObjectVar::FromPPVar(var));
      if (!object.get()) {
        VOID_TO_NPVARIANT(*result);
        return false;
      }
      OBJECT_TO_NPVARIANT(object->np_object(), *result);
      break;
    }
    default:
      VOID_TO_NPVARIANT(*result);
      return false;
  }
  return true;
}

// ObjectAccessorTryCatch ------------------------------------------------------

// Automatically sets up a TryCatch for accessing the object identified by the
// given PP_Var. The module from the object will be used for the exception
// strings generated by the TryCatch.
//
// This will automatically retrieve the ObjectVar from the object and throw
// an exception if it's invalid. At the end of construction, if there is no
// exception, you know that there is no previously set exception, that the
// object passed in is valid and ready to use (via the object() getter), and
// that the TryCatch's pp_module() getter is also set up properly and ready to
// use.
class ObjectAccessorTryCatch : public TryCatch {
 public:
  ObjectAccessorTryCatch(PP_Var object, PP_Var* exception)
      : TryCatch(exception),
        object_(NPObjectVar::FromPPVar(object)) {
    if (!object_.get()) {
      SetException(kInvalidObjectException);
    }
  }

  NPObjectVar* object() { return object_.get(); }

  PepperPluginInstanceImpl* GetPluginInstance() {
    return HostGlobals::Get()->GetInstance(object()->pp_instance());
  }

 protected:
  scoped_refptr<NPObjectVar> object_;

  DISALLOW_COPY_AND_ASSIGN(ObjectAccessorTryCatch);
};

// ObjectAccessiorWithIdentifierTryCatch ---------------------------------------

// Automatically sets up a TryCatch for accessing the identifier on the given
// object. This just extends ObjectAccessorTryCatch to additionally convert
// the given identifier to an NPIdentifier and validate it, throwing an
// exception if it's invalid.
//
// At the end of construction, if there is no exception, you know that there is
// no previously set exception, that the object passed in is valid and ready to
// use (via the object() getter), that the identifier is valid and ready to
// use (via the identifier() getter), and that the TryCatch's pp_module() getter
// is also set up properly and ready to use.
class ObjectAccessorWithIdentifierTryCatch : public ObjectAccessorTryCatch {
 public:
  ObjectAccessorWithIdentifierTryCatch(PP_Var object,
                                       PP_Var identifier,
                                       PP_Var* exception)
      : ObjectAccessorTryCatch(object, exception),
        identifier_(0) {
    if (!has_exception()) {
      identifier_ = PPVarToNPIdentifier(identifier);
      if (!identifier_)
        SetException(kInvalidPropertyException);
    }
  }

  NPIdentifier identifier() const { return identifier_; }

 private:
  NPIdentifier identifier_;

  DISALLOW_COPY_AND_ASSIGN(ObjectAccessorWithIdentifierTryCatch);
};

PP_Bool HasProperty(PP_Var var,
                    PP_Var name,
                    PP_Var* exception) {
  ObjectAccessorWithIdentifierTryCatch accessor(var, name, exception);
  if (accessor.has_exception())
    return PP_FALSE;
  return BoolToPPBool(WebBindings::hasProperty(NULL,
                                               accessor.object()->np_object(),
                                               accessor.identifier()));
}

bool HasPropertyDeprecated(PP_Var var,
                           PP_Var name,
                           PP_Var* exception) {
  return PPBoolToBool(HasProperty(var, name, exception));
}

bool HasMethodDeprecated(PP_Var var,
                         PP_Var name,
                         PP_Var* exception) {
  ObjectAccessorWithIdentifierTryCatch accessor(var, name, exception);
  if (accessor.has_exception())
    return false;
  return WebBindings::hasMethod(NULL, accessor.object()->np_object(),
                                accessor.identifier());
}

PP_Var GetProperty(PP_Var var,
                   PP_Var name,
                   PP_Var* exception) {
  ObjectAccessorWithIdentifierTryCatch accessor(var, name, exception);
  if (accessor.has_exception())
    return PP_MakeUndefined();

  NPVariant result;
  if (!WebBindings::getProperty(NULL, accessor.object()->np_object(),
                                accessor.identifier(), &result)) {
    // An exception may have been raised.
    accessor.SetException(kUnableToGetPropertyException);
    return PP_MakeUndefined();
  }

  PP_Var ret = NPVariantToPPVar(accessor.GetPluginInstance(), &result);
  WebBindings::releaseVariantValue(&result);
  return ret;
}

void EnumerateProperties(PP_Var var,
                         uint32_t* property_count,
                         PP_Var** properties,
                         PP_Var* exception) {
  *properties = NULL;
  *property_count = 0;

  ObjectAccessorTryCatch accessor(var, exception);
  if (accessor.has_exception())
    return;

  NPIdentifier* identifiers = NULL;
  uint32_t count = 0;
  if (!WebBindings::enumerate(NULL, accessor.object()->np_object(),
                              &identifiers, &count)) {
    accessor.SetException(kUnableToGetAllPropertiesException);
    return;
  }

  if (count == 0)
    return;

  *property_count = count;
  *properties = static_cast<PP_Var*>(malloc(sizeof(PP_Var) * count));
  for (uint32_t i = 0; i < count; ++i) {
    (*properties)[i] = NPIdentifierToPPVar(identifiers[i]);
  }
  free(identifiers);
}

void SetPropertyDeprecated(PP_Var var,
                           PP_Var name,
                           PP_Var value,
                           PP_Var* exception) {
  ObjectAccessorWithIdentifierTryCatch accessor(var, name, exception);
  if (accessor.has_exception())
    return;

  NPVariant variant;
  if (!PPVarToNPVariantNoCopy(value, &variant)) {
    accessor.SetException(kInvalidValueException);
    return;
  }
  if (!WebBindings::setProperty(NULL, accessor.object()->np_object(),
                                accessor.identifier(), &variant))
    accessor.SetException(kUnableToSetPropertyException);
}

void DeletePropertyDeprecated(PP_Var var,
                              PP_Var name,
                              PP_Var* exception) {
  ObjectAccessorWithIdentifierTryCatch accessor(var, name, exception);
  if (accessor.has_exception())
    return;

  if (!WebBindings::removeProperty(NULL, accessor.object()->np_object(),
                                   accessor.identifier()))
    accessor.SetException(kUnableToRemovePropertyException);
}

PP_Var InternalCallDeprecated(ObjectAccessorTryCatch* accessor,
                              PP_Var method_name,
                              uint32_t argc,
                              PP_Var* argv,
                              PP_Var* exception) {
  NPIdentifier identifier;
  if (method_name.type == PP_VARTYPE_UNDEFINED) {
    identifier = NULL;
  } else if (method_name.type == PP_VARTYPE_STRING) {
    // Specifically allow only string functions to be called.
    identifier = PPVarToNPIdentifier(method_name);
    if (!identifier) {
      accessor->SetException(kInvalidPropertyException);
      return PP_MakeUndefined();
    }
  } else {
    accessor->SetException(kInvalidPropertyException);
    return PP_MakeUndefined();
  }

  scoped_ptr<NPVariant[]> args;
  if (argc) {
    args.reset(new NPVariant[argc]);
    for (uint32_t i = 0; i < argc; ++i) {
      if (!PPVarToNPVariantNoCopy(argv[i], &args[i])) {
        // This argument was invalid, throw an exception & give up.
        accessor->SetException(kInvalidValueException);
        return PP_MakeUndefined();
      }
    }
  }

  bool ok;

  NPVariant result;
  if (identifier) {
    ok = WebBindings::invoke(NULL, accessor->object()->np_object(),
                             identifier, args.get(), argc, &result);
  } else {
    ok = WebBindings::invokeDefault(NULL, accessor->object()->np_object(),
                                    args.get(), argc, &result);
  }

  if (!ok) {
    // An exception may have been raised.
    accessor->SetException(kUnableToCallMethodException);
    return PP_MakeUndefined();
  }

  PP_Var ret = NPVariantToPPVar(accessor->GetPluginInstance(), &result);
  WebBindings::releaseVariantValue(&result);
  return ret;
}

PP_Var CallDeprecated(PP_Var var,
                      PP_Var method_name,
                      uint32_t argc,
                      PP_Var* argv,
                      PP_Var* exception) {
  ObjectAccessorTryCatch accessor(var, exception);
  if (accessor.has_exception())
    return PP_MakeUndefined();
  PepperPluginInstanceImpl* plugin = accessor.GetPluginInstance();
  if (plugin && plugin->IsProcessingUserGesture()) {
    blink::WebScopedUserGesture user_gesture(
        plugin->CurrentUserGestureToken());
    return InternalCallDeprecated(&accessor, method_name, argc, argv,
                                  exception);
  }
  return InternalCallDeprecated(&accessor, method_name, argc, argv, exception);
}

PP_Var Construct(PP_Var var,
                 uint32_t argc,
                 PP_Var* argv,
                 PP_Var* exception) {
  ObjectAccessorTryCatch accessor(var, exception);
  if (accessor.has_exception())
    return PP_MakeUndefined();

  scoped_ptr<NPVariant[]> args;
  if (argc) {
    args.reset(new NPVariant[argc]);
    for (uint32_t i = 0; i < argc; ++i) {
      if (!PPVarToNPVariantNoCopy(argv[i], &args[i])) {
        // This argument was invalid, throw an exception & give up.
        accessor.SetException(kInvalidValueException);
        return PP_MakeUndefined();
      }
    }
  }

  NPVariant result;
  if (!WebBindings::construct(NULL, accessor.object()->np_object(),
                              args.get(), argc, &result)) {
    // An exception may have been raised.
    accessor.SetException(kUnableToConstructException);
    return PP_MakeUndefined();
  }

  PP_Var ret = NPVariantToPPVar(accessor.GetPluginInstance(), &result);
  WebBindings::releaseVariantValue(&result);
  return ret;
}

bool IsInstanceOfDeprecated(PP_Var var,
                            const PPP_Class_Deprecated* ppp_class,
                            void** ppp_class_data) {
  scoped_refptr<NPObjectVar> object(NPObjectVar::FromPPVar(var));
  if (!object.get())
    return false;  // Not an object at all.

  return PluginObject::IsInstanceOf(object->np_object(),
                                    ppp_class, ppp_class_data);
}

PP_Var CreateObjectDeprecated(PP_Instance pp_instance,
                              const PPP_Class_Deprecated* ppp_class,
                              void* ppp_class_data) {
  PepperPluginInstanceImpl* instance =
      HostGlobals::Get()->GetInstance(pp_instance);
  if (!instance) {
    DLOG(ERROR) << "Create object passed an invalid instance.";
    return PP_MakeNull();
  }
  return PluginObject::Create(instance, ppp_class, ppp_class_data);
}

PP_Var CreateObjectWithModuleDeprecated(PP_Module pp_module,
                                        const PPP_Class_Deprecated* ppp_class,
                                        void* ppp_class_data) {
  PluginModule* module = HostGlobals::Get()->GetModule(pp_module);
  if (!module)
    return PP_MakeNull();
  return PluginObject::Create(module->GetSomeInstance(),
                              ppp_class, ppp_class_data);
}

}  // namespace

// static
const PPB_Var_Deprecated* PPB_Var_Deprecated_Impl::GetVarDeprecatedInterface() {
  static const PPB_Var_Deprecated var_deprecated_interface = {
    ppapi::PPB_Var_Shared::GetVarInterface1_0()->AddRef,
    ppapi::PPB_Var_Shared::GetVarInterface1_0()->Release,
    ppapi::PPB_Var_Shared::GetVarInterface1_0()->VarFromUtf8,
    ppapi::PPB_Var_Shared::GetVarInterface1_0()->VarToUtf8,
    &HasPropertyDeprecated,
    &HasMethodDeprecated,
    &GetProperty,
    &EnumerateProperties,
    &SetPropertyDeprecated,
    &DeletePropertyDeprecated,
    &CallDeprecated,
    &Construct,
    &IsInstanceOfDeprecated,
    &CreateObjectDeprecated,
    &CreateObjectWithModuleDeprecated,
  };

  return &var_deprecated_interface;
}

}  // namespace content

