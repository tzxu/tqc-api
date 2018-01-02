/* DO NOT EDIT THIS FILE - it is machine generated */
#include <jni.h>
/* Header for class com_acqusta_tquant_api_impl_DataApiJni */

#ifndef _Included_com_acqusta_tquant_api_impl_DataApiJni
#define _Included_com_acqusta_tquant_api_impl_DataApiJni
#ifdef __cplusplus
extern "C" {
#endif
/*
 * Class:     com_acqusta_tquant_api_impl_DataApiJni
 * Method:    getTick
 * Signature: (JLjava/lang/String;ILjava/lang/String;)[Lcom/acqusta/tquant/api/DataApi/MarketQuote;
 */
JNIEXPORT jobjectArray JNICALL Java_com_acqusta_tquant_api_impl_DataApiJni_getTick
  (JNIEnv *, jclass, jlong, jstring, jint, jstring);

/*
 * Class:     com_acqusta_tquant_api_impl_DataApiJni
 * Method:    getBar
 * Signature: (JLjava/lang/String;Ljava/lang/String;IZLjava/lang/String;)[Lcom/acqusta/tquant/api/DataApi/Bar;
 */
JNIEXPORT jobjectArray JNICALL Java_com_acqusta_tquant_api_impl_DataApiJni_getBar
  (JNIEnv *, jclass, jlong, jstring, jstring, jint, jboolean, jstring);

/*
 * Class:     com_acqusta_tquant_api_impl_DataApiJni
 * Method:    getDailyBar
 * Signature: (JLjava/lang/String;Ljava/lang/String;ZLjava/lang/String;)[Lcom/acqusta/tquant/api/DataApi/DailyBar;
 */
JNIEXPORT jobjectArray JNICALL Java_com_acqusta_tquant_api_impl_DataApiJni_getDailyBar
  (JNIEnv *, jclass, jlong, jstring, jstring, jboolean, jstring);

/*
 * Class:     com_acqusta_tquant_api_impl_DataApiJni
 * Method:    getQuote
 * Signature: (JLjava/lang/String;Ljava/lang/String;)Lcom/acqusta/tquant/api/DataApi/MarketQuote;
 */
JNIEXPORT jobject JNICALL Java_com_acqusta_tquant_api_impl_DataApiJni_getQuote
  (JNIEnv *, jclass, jlong, jstring, jstring);

/*
 * Class:     com_acqusta_tquant_api_impl_DataApiJni
 * Method:    subscribe
 * Signature: (J[Ljava/lang/String;Ljava/lang/String;)[Ljava/lang/String;
 */
JNIEXPORT jobjectArray JNICALL Java_com_acqusta_tquant_api_impl_DataApiJni_subscribe
  (JNIEnv *, jclass, jlong, jobjectArray, jstring);

/*
 * Class:     com_acqusta_tquant_api_impl_DataApiJni
 * Method:    unsubscribe
 * Signature: (J[Ljava/lang/String;Ljava/lang/String;)[Ljava/lang/String;
 */
JNIEXPORT jobjectArray JNICALL Java_com_acqusta_tquant_api_impl_DataApiJni_unsubscribe
  (JNIEnv *, jclass, jlong, jobjectArray, jstring);

/*
 * Class:     com_acqusta_tquant_api_impl_DataApiJni
 * Method:    setCallback
 * Signature: (JLcom/acqusta/tquant/api/DataApi/Callback;)V
 */
JNIEXPORT void JNICALL Java_com_acqusta_tquant_api_impl_DataApiJni_setCallback
  (JNIEnv *, jclass, jlong, jobject);

#ifdef __cplusplus
}
#endif
#endif