/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is RaptorCanvas.
 *
 * The Initial Developer of the Original Code is Kirk Baker and
 * Ian Wilkinson. Portions created by Kirk Baker and Ian Wilkinson are
 * Copyright (C) 1999 Kirk Baker and Ian Wilkinson. All
 * Rights Reserved.
 *
 * Contributor(s): Kirk Baker <kbaker@eb.com>
 *               Ian Wilkinson <iw@ennoble.com>
 *               Mark Lin <mark.lin@eng.sun.com>
 *               Mark Goddard
 *               Ed Burns <edburns@acm.org>
 *               Ann Sunhachawee
 */

#include "prlog.h" // for PR_ASSERT

#include "jni_util.h"

JavaVM *gVm = nsnull; // declared in ns_globals.h, which is included in
                      // jni_util.h

//
// Local cache variables of JNI data items
//

static jclass gPropertiesClass = nsnull;
static jmethodID gPropertiesInitMethodID = nsnull;
static jmethodID gPropertiesSetPropertyMethodID = nsnull;

void util_ThrowExceptionToJava (JNIEnv * env, const char * message)
{
    if (env->ExceptionOccurred()) {
      env->ExceptionClear();
    }
    jclass excCls = env->FindClass("java/lang/Exception");
    
    if (excCls == 0) { // Unable to find the exception class, give up.
	if (env->ExceptionOccurred())
	    env->ExceptionClear();
	return;
    }
    
    // Throw the exception with the error code and description
    jmethodID jID = env->GetMethodID(excCls, "<init>", "(Ljava/lang/String;)V");		// void Exception(String)
    
    if (jID != nsnull) {
	jstring	exceptionString = env->NewStringUTF(message);
	jthrowable newException = (jthrowable) env->NewObject(excCls, jID, exceptionString);
        
        if (newException != nsnull) {
	    env->Throw(newException);
	}
	else {
	    if (env->ExceptionOccurred())
		env->ExceptionClear();
	}
	} else {
	    if (env->ExceptionOccurred())
		env->ExceptionClear();
	}
} // ThrowExceptionToJava()

void util_PostEvent(WebShellInitContext * initContext, PLEvent * event)
{
    PL_ENTER_EVENT_QUEUE_MONITOR(initContext->actionQueue);
  
    ::PL_PostEvent(initContext->actionQueue, event);
    
    PL_EXIT_EVENT_QUEUE_MONITOR(initContext->actionQueue);
} // PostEvent()


void *util_PostSynchronousEvent(WebShellInitContext * initContext, PLEvent * event)
{
    void    *       voidResult = nsnull;

    PL_ENTER_EVENT_QUEUE_MONITOR(initContext->actionQueue);
    
    voidResult = ::PL_PostSynchronousEvent(initContext->actionQueue, event);
    
    PL_EXIT_EVENT_QUEUE_MONITOR(initContext->actionQueue);
    
    return voidResult;
} // PostSynchronousEvent()          


void util_SendEventToJava(JNIEnv *yourEnv, jobject nativeEventThread,
                          jobject webclientEventListener, 
                          jlong eventType, jobject eventData)
{
#ifdef BAL_INTERFACE
    if (nsnull != externalEventOccurred) {
        externalEventOccurred(yourEnv, nativeEventThread,
                              webclientEventListener, eventType, eventData);
    }
#else
    if (nsnull == gVm) {
        return;
    }

	JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION_1_2);

    if (nsnull == env) {
      return;
    }

    jthrowable exception;

    if (nsnull != (exception = env->ExceptionOccurred())) {
        env->ExceptionDescribe();
    }

    jclass clazz = env->GetObjectClass(nativeEventThread);
    jmethodID mid = env->GetMethodID(clazz, "nativeEventOccurred", 
                                     "(Lorg/mozilla/webclient/WebclientEventListener;JLjava/lang/Object;)V");
    if ( mid != nsnull) {
        env->CallVoidMethod(nativeEventThread, mid, webclientEventListener,
                            eventType, eventData);
    } else {
        if (prLogModuleInfo) {
            PR_LOG(prLogModuleInfo, 3, 
                   ("cannot call the Java Method!\n"));
        }
    }
#endif
}

/**

 * @return: the string name of the current java thread.  Must be freed!

 */

char *util_GetCurrentThreadName(JNIEnv *env)
{
	jclass threadClass = env->FindClass("java/lang/Thread");
	jobject currentThread;
	jstring name;
	char *result = nsnull;
	const char *cstr;
	jboolean isCopy;

	if (nsnull != threadClass) {
		jmethodID currentThreadID = 
			env->GetStaticMethodID(threadClass,"currentThread",
								   "()Ljava/lang/Thread;");
		currentThread = env->CallStaticObjectMethod(threadClass, 
													currentThreadID);
		if (nsnull != currentThread) {
			jmethodID getNameID = env->GetMethodID(threadClass,
												   "getName",
												   "()Ljava/lang/String;");
			name = (jstring) env->CallObjectMethod(currentThread, getNameID, 
												   nsnull);
			result = strdup(cstr = env->GetStringUTFChars(name, &isCopy));
			if (JNI_TRUE == isCopy) {
				env->ReleaseStringUTFChars(name, cstr);
			}
		}
	}
	return result;
}

// static
void util_DumpJavaStack(JNIEnv *env)
{
	jclass threadClass = env->FindClass("java/lang/Thread");
	if (nsnull != threadClass) {
		jmethodID dumpStackID = env->GetStaticMethodID(threadClass,
													   "dumpStack",
													   "()V");
		env->CallStaticVoidMethod(threadClass, dumpStackID);
	}
}

jobject util_NewGlobalRef(JNIEnv *env, jobject obj)
{
    jobject result = nsnull;
#ifdef BAL_INTERFACE
    // PENDING(edburns): do we need to do anything here?
    result = obj;
#else
    result = env->NewGlobalRef(obj);
#endif
    return result;
}

void util_DeleteGlobalRef(JNIEnv *env, jobject obj)
{
#ifdef BAL_INTERFACE
#else
    env->DeleteGlobalRef(obj);
#endif
}

jthrowable util_ExceptionOccurred(JNIEnv *env)
{
    jthrowable result = nsnull;
#ifdef BAL_INTERFACE
#else
    result = env->ExceptionOccurred();
#endif
    return result;
}

jint util_GetJavaVM(JNIEnv *env, JavaVM **vm)
{
    int result = -1;
#ifdef BAL_INTERFACE
#else
    result = env->GetJavaVM(vm);
#endif
    return result;
}

jclass util_FindClass(JNIEnv *env, const char *fullyQualifiedClassName)
{
    jclass result = nsnull;
#ifdef BAL_INTERFACE
    result = util_GetClassMapping(fullyQualifiedClassName);
#else
    result = env->FindClass(fullyQualifiedClassName);
#endif
    return result;
}

jfieldID util_GetStaticFieldID(JNIEnv *env, jclass clazz, 
                               const char *fieldName, 
                               const char *signature)
{
    jfieldID result = nsnull;
#ifdef BAL_INTERFACE
#else
    result = env->GetStaticFieldID(clazz, fieldName, signature);
#endif
    return result;
}

jlong util_GetStaticLongField(JNIEnv *env, jclass clazz, jfieldID id)
{
    jlong result = -1;
#ifdef BAL_INTERFACE
#else
    result = env->GetStaticLongField(clazz, id);
#endif
    return result;
}

jboolean util_IsInstanceOf(JNIEnv *env, jobject obj, jclass clazz)
{
    jboolean result = JNI_FALSE;
#ifdef BAL_INTERFACE
    if (nsnull != externalInstanceOf) {
        result = externalInstanceOf(env, obj, clazz);
    }
#else
    result = env->IsInstanceOf(obj, clazz);
#endif
    return result;
}

#ifdef XP_UNIX
jint util_GetGTKWinPtrFromCanvas(JNIEnv *env, jobject browserControlCanvas)
{
    jint result = -1;
#ifdef BAL_INTERFACE
#else
    jclass cls = env->GetObjectClass(browserControlCanvas);  // Get Class for BrowserControlImpl object
    jclass clz = env->FindClass("org/mozilla/webclient/BrowserControlImpl");
    if (nsnull == clz) {
        ::util_ThrowExceptionToJava(env, "Exception: Could not find class for BrowserControlImpl");
        return (jint) 0;
    }
    jboolean ans = env->IsInstanceOf(browserControlCanvas, clz);
    if (JNI_FALSE == ans) {
        ::util_ThrowExceptionToJava(env, "Exception: We have a problem");
        return (jint) 0;
    }
    // Get myCanvas IVar
    jfieldID fid = env->GetFieldID(cls, "myCanvas", "Lorg/mozilla/webclient/BrowserControlCanvas;");
    if (nsnull == fid) {
        ::util_ThrowExceptionToJava(env, "Exception: field myCanvas not found in the jobject for BrowserControlImpl");
        return (jint) 0;
    }
    jobject canvasObj = env->GetObjectField(browserControlCanvas, fid);
    jclass canvasCls = env->GetObjectClass(canvasObj);
    if (nsnull == canvasCls) {
        ::util_ThrowExceptionToJava(env, "Exception: Could Not find Class for CanvasObj");
        return (jint) 0;
    }
    jfieldID gtkfid = env->GetFieldID(canvasCls, "gtkWinPtr", "I");
    if (nsnull == gtkfid) {
        ::util_ThrowExceptionToJava(env, "Exception: field gtkWinPtr not found in the jobject for BrowserControlCanvas");
        return (jint) 0;
    }
    result = env->GetIntField(canvasObj, gtkfid);
#endif
    return result;
}
#endif

jint util_GetIntValueFromInstance(JNIEnv *env, jobject obj,
                                  const char *fieldName)
{
    int result = -1;
#ifdef BAL_INTERFACE
#else
    jclass objClass = env->GetObjectClass(obj);
    if (nsnull == objClass) {
        if (prLogModuleInfo) {
            PR_LOG(prLogModuleInfo, 3, 
                   ("util_GetIntValueFromInstance: Can't get object class from instance.\n"));
        }
        return result;
    }

    jfieldID theFieldID = env->GetFieldID(objClass, fieldName, "I");
    if (nsnull == theFieldID) {
        if (prLogModuleInfo) {
            PR_LOG(prLogModuleInfo, 3, 
                   ("util_GetIntValueFromInstance: Can't get fieldID for fieldName.\n"));
        }
        return result;
    }

    result = env->GetIntField(obj, theFieldID);
#endif
    return result;
}

void util_SetIntValueForInstance(JNIEnv *env, jobject obj,
                                 const char *fieldName, jint newValue)
{
#ifdef BAL_INTERFACE
#else
    jclass objClass = env->GetObjectClass(obj);
    if (nsnull == objClass) {
        if (prLogModuleInfo) {
            PR_LOG(prLogModuleInfo, 3, 
                   ("util_SetIntValueForInstance: Can't get object class from instance.\n"));
        }
        return;
    }

    jfieldID fieldID = env->GetFieldID(objClass, fieldName, "I");
    if (nsnull == fieldID) {
        if (prLogModuleInfo) {
            PR_LOG(prLogModuleInfo, 3, 
                   ("util_SetIntValueForInstance: Can't get fieldID for fieldName.\n"));
        }
        return;
    }
    
    env->SetIntField(obj, fieldID, newValue);
#endif;
}

jobject util_CreatePropertiesObject(JNIEnv *env, jobject reserved_NotUsed)
{
    jobject result = nsnull;
#ifdef BAL_INTERFACE
    if (nsnull != externalCreatePropertiesObject) {
        result = externalCreatePropertiesObject(env, reserved_NotUsed);
    }
#else
    // For some reason, we have to do FindClass each time.  If we try to
    // cache the class, it crashes.  I think this may have something to
    // do with threading issues.
    if (nsnull == (gPropertiesClass 
                   = ::util_FindClass(env, "java/util/Properties"))) {
        return result;
    }
    
    if (nsnull == gPropertiesInitMethodID) {
        if (nsnull == (gPropertiesInitMethodID = 
                       env->GetMethodID(gPropertiesClass, "<init>", "()V"))) {
            return result;
        }
    }

    result = env->NewObject(gPropertiesClass, gPropertiesInitMethodID);

#endif
    return result;
}

void util_DestroyPropertiesObject(JNIEnv *env, jobject propertiesObject,
                                  jobject reserved_NotUsed)
{
#ifdef BAL_INTERFACE
    if (nsnull != externalDestroyPropertiesObject) {
        externalDestroyPropertiesObject(env, propertiesObject, 
                                        reserved_NotUsed);
    }
#else
#endif
}

void util_StoreIntoPropertiesObject(JNIEnv *env, jobject propertiesObject,
                                    jobject name, jobject value)
{
#ifdef BAL_INTERFACE
    if (nsnull != externalStoreIntoPropertiesObject) {
        externalStoreIntoPropertiesObject(env, propertiesObject, name, value);
    }
#else
    if (nsnull == gPropertiesSetPropertyMethodID) {
        PR_ASSERT(gPropertiesClass);
        if (nsnull == (gPropertiesSetPropertyMethodID = 
                       env->GetMethodID(gPropertiesClass, 
                                        "setProperty",
                                        "(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/Object;"))) {
            return;
        }
    }
    PR_ASSERT(gPropertiesSetPropertyMethodID);

    env->CallObjectMethod(propertiesObject, gPropertiesSetPropertyMethodID,
                          name, value);
                       

    
#endif
}

JNIEXPORT jvalue JNICALL
JNU_CallMethodByName(JNIEnv *env, 
					 jboolean *hasException,
					 jobject obj, 
					 const char *name,
					 const char *signature,
					 ...)
{
    jvalue result;
    va_list args;
	
    va_start(args, signature);
    result = JNU_CallMethodByNameV(env, hasException, obj, name, signature, 
								   args); 
    va_end(args);
	
    return result;    
}

JNIEXPORT jvalue JNICALL
JNU_CallMethodByNameV(JNIEnv *env, 
					  jboolean *hasException,
					  jobject obj, 
					  const char *name,
					  const char *signature, 
					  va_list args)
{
    jclass clazz;
    jmethodID mid;
    jvalue result;
    const char *p = signature;
    /* find out the return type */
    while (*p && *p != ')')
        p++;
    p++;
	
    result.i = 0;
	
    if (env->EnsureLocalCapacity(3) < 0)
        goto done2;
    clazz = env->GetObjectClass(obj);
    mid = env->GetMethodID(clazz, name, signature);
    if (mid == 0)
        goto done1;
    switch (*p) {
    case 'V':
        env->CallVoidMethodV(obj, mid, args);
		break;
    case '[':
    case 'L':
        result.l = env->CallObjectMethodV(obj, mid, args);
		break;
    case 'Z':
        result.z = env->CallBooleanMethodV(obj, mid, args);
		break;
    case 'B':
        result.b = env->CallByteMethodV(obj, mid, args);
		break;
    case 'C':
        result.c = env->CallCharMethodV(obj, mid, args);
		break;
    case 'S':
        result.s = env->CallShortMethodV(obj, mid, args);
		break;
    case 'I':
        result.i = env->CallIntMethodV(obj, mid, args);
		break;
    case 'J':
        result.j = env->CallLongMethodV(obj, mid, args);
		break;
    case 'F':
        result.f = env->CallFloatMethodV(obj, mid, args);
		break;
    case 'D':
        result.d = env->CallDoubleMethodV(obj, mid, args);
		break;
    default:
        env->FatalError("JNU_CallMethodByNameV: illegal signature");
    }
  done1:
    env->DeleteLocalRef(clazz);
  done2:
    if (hasException) {
        *hasException = env->ExceptionCheck();
    }
    return result;    
}

JNIEXPORT void * JNICALL
JNU_GetEnv(JavaVM *vm, jint version)
{
    //    void *result;
    //vm->GetEnv(&result, version);
    JNIEnv *result = nsnull;
#ifdef BAL_INTERFACE
#else
    vm->AttachCurrentThread((void **)&result, (void *) version);
#endif
    return result;
}

