//
// Created by lizhi on 2022/3/23.
//
#include <xquic_common.h>
#include "native_xquic_short.h"
#include "xquic_client_short.h"

/**
 * meg type
 */
typedef enum client_msg_type {
    MSG_TYPE_TOKEN,//token
    MSG_TYPE_SESSION,//session
    MSG_TYPE_TP,//tp
    MSG_TYPE_DATA//service rev data
} MSG_TYPE;


void
callback_to_java(void *ev_android, void *object_android, int msg_type, const unsigned char *data,
                 unsigned len) {
    if (len <= 0) {
        LOGW("call back java error,can len = %d", len);
        return;
    }
    JNIEnv *env = (JNIEnv *) ev_android;

    /* find class and get method */
    jclass callbackClass = (*env)->GetObjectClass(env, object_android);
    jobject j_obj = (*env)->NewGlobalRef(env, object_android);//关键，要不会崩溃
    jmethodID jmid = (*env)->GetMethodID(env, callbackClass, "callBackMessage", "(I[B)V");
    if (!jmid) {
        LOGE("call back java error,can not find methodId callBackMessage");
        return;
    }

    /* data to byteArray*/
    jbyteArray dataBuf = (*env)->NewByteArray(env, len);
    (*env)->SetByteArrayRegion(env, dataBuf, 0, len, (jbyte *) data);

    /* call back */
    (*env)->CallVoidMethod(env, j_obj, jmid, msg_type, dataBuf);

    /* free */
    (*env)->DeleteGlobalRef(env, j_obj);
    (*env)->DeleteLocalRef(env, dataBuf);
    (*env)->DeleteLocalRef(env, callbackClass);
}

void callback_token(void *ev_android, void *object_android, const unsigned char *data,
                    unsigned len) {
    DEBUG;
    callback_to_java(ev_android, object_android, MSG_TYPE_TOKEN, data, len);
}

void callback_session(void *ev_android, void *object_android, const char *data, size_t len) {
    DEBUG;
    callback_to_java(ev_android, object_android, MSG_TYPE_SESSION, data, len);
}


void callback_tp(void *ev_android, void *object_android, const char *data, size_t len) {
    DEBUG;
    callback_to_java(ev_android, object_android, MSG_TYPE_TP, data, len);
}


/**
 *
 * @return callback data to java
 */
int callback_read_data(void *ev_android, void *object_android, int ret, char *data, ssize_t len) {
    JNIEnv *env = (JNIEnv *) ev_android;

    /* find class and get method */
    jclass callbackClass = (*env)->GetObjectClass(env, object_android);
    jobject j_obj = (*env)->NewGlobalRef(env, object_android);//关键，要不会崩溃
    jmethodID jmid = (*env)->GetMethodID(env, callbackClass, "callBackReadData", "(I[B)V");
    if (!jmid) {
        LOGE("call back error,can not find methodId callBackReadData");
        return -1;
    }

    /* data to byteArray*/
    jbyteArray dataBuf = (*env)->NewByteArray(env, len);
    (*env)->SetByteArrayRegion(env, dataBuf, 0, len, (jbyte *) data);

    /* call back */
    (*env)->CallVoidMethod(env, j_obj, jmid, ret, dataBuf);

    /* free */
    (*env)->DeleteGlobalRef(env, j_obj);
    (*env)->DeleteLocalRef(env, dataBuf);
    (*env)->DeleteLocalRef(env, callbackClass);
    return 0;
}

jstring getString(JNIEnv *env, jobject param, const char *field) {
    jclass sendParamsClass = (*env)->GetObjectClass(env, param);
    jfieldID jfieldId = (*env)->GetFieldID(env, sendParamsClass, field, "Ljava/lang/String;");
    if (!jfieldId) {
        return NULL;
    }
    jstring string = (jstring) (*env)->GetObjectField(env, param, jfieldId);
    (*env)->DeleteLocalRef(env, sendParamsClass);
    return string;
}

jint getInt(JNIEnv *env, jobject param, const char *field) {
    jclass sendParamsClass = (*env)->GetObjectClass(env, param);
    jfieldID jfieldId = (*env)->GetFieldID(env, sendParamsClass, field, "I");
    if (!jfieldId) {
        return 0;
    }
    jint data = (*env)->GetIntField(env, param, jfieldId);
    (*env)->DeleteLocalRef(env, sendParamsClass);
    return data;
}


int build_headers_from_params(JNIEnv *env, jobject param, const char *field,
                              xqc_http_header_t *heards) {
    jclass sendParamsClass = (*env)->GetObjectClass(env, param);
    jfieldID jfieldId = (*env)->GetFieldID(env, sendParamsClass, field, "Ljava/util/HashMap;");
    if (!jfieldId) {
        return -1;
    }
    jobject headersHashMapObject = (*env)->GetObjectField(env, param, jfieldId);
    if (!headersHashMapObject) {
        return -1;
    }

    jclass hashMapClass = (*env)->FindClass(env, "java/util/HashMap");
    jmethodID entrySetMID = (*env)->GetMethodID(env, hashMapClass, "entrySet", "()Ljava/util/Set;");

    jobject setObj = (*env)->CallObjectMethod(env, headersHashMapObject, entrySetMID);
    jclass setClass = (*env)->FindClass(env, "java/util/Set");
    jmethodID iteratorMID = (*env)->GetMethodID(env, setClass, "iterator",
                                                "()Ljava/util/Iterator;");

    jobject iteratorObj = (*env)->CallObjectMethod(env, setObj, iteratorMID);
    jclass iteratorClass = (*env)->FindClass(env, "java/util/Iterator");
    jmethodID hashNextMID = (*env)->GetMethodID(env, iteratorClass, "hasNext", "()Z");
    jmethodID nextMID = (*env)->GetMethodID(env, iteratorClass, "next", "()Ljava/lang/Object;");

    jclass entryClass = (*env)->FindClass(env, "java/util/Map$Entry");
    jmethodID getKeyMID = (*env)->GetMethodID(env, entryClass, "getKey", "()Ljava/lang/Object;");
    jmethodID getValueMID = (*env)->GetMethodID(env, entryClass, "getValue",
                                                "()Ljava/lang/Object;");

    int i = 0;
    while ((*env)->CallBooleanMethod(env, iteratorObj, hashNextMID)) {
        jobject entryObj = (*env)->CallObjectMethod(env, iteratorObj, nextMID);

        jstring keyString = (*env)->CallObjectMethod(env, entryObj, getKeyMID);
        const char *keyChar = (*env)->GetStringUTFChars(env, keyString, NULL);

        jstring valueString = (*env)->CallObjectMethod(env, entryObj, getValueMID);
        const char *valueChar = (*env)->GetStringUTFChars(env, valueString, NULL);

        xqc_http_header_t header = {
                .name = {.iov_base = keyChar, .iov_len = strlen(keyChar)},
                .value = {.iov_base = valueChar, .iov_len = strlen(valueChar)},
                .flags = 0,
        };
        heards[i] = header;
        i++;
    }

    (*env)->DeleteLocalRef(env, sendParamsClass);
    (*env)->DeleteLocalRef(env, headersHashMapObject);
    (*env)->DeleteLocalRef(env, hashMapClass);
    (*env)->DeleteLocalRef(env, setObj);
    (*env)->DeleteLocalRef(env, setClass);
    (*env)->DeleteLocalRef(env, iteratorObj);
    (*env)->DeleteLocalRef(env, iteratorClass);
    (*env)->DeleteLocalRef(env, entryClass);

    return 0;
}


/*
 * get params
 */
xqc_cli_user_data_params_t *get_data_params(JNIEnv *env, jobject param, jobject callback) {

    jstring url = getString(env, param, "url");

    if (url == NULL) {
        LOGE("xquicConnect error url == NULL");
        return NULL;
    }

    jstring token = getString(env, param, "token");
    jstring session = getString(env, param, "session");
    jint time_out = getInt(env, param, "timeOut");
    jint max_recv_data_len = getInt(env, param, "maxRecvDataLen");
    jint cc_type = getInt(env, param, "ccType");
    jint headersSize = getInt(env, param, "headersSize");

    /* build header from params */
    xqc_http_header_t *headers = malloc(sizeof(xqc_http_header_t) * headersSize);
    if (build_headers_from_params(env, param, "headers", headers) < 0) {
        LOGE("build_headers_from_params error");
        return NULL;
    }

    const char *cUrl = (*env)->GetStringUTFChars(env, url, 0);

    jstring content = getString(env, param, "content");
    const char *cContent = NULL;
    if (content != NULL) {
        cContent = (*env)->GetStringUTFChars(env, content, 0);
    }

    const char *cToken = NULL;
    if (token != NULL) {
        cToken = (*env)->GetStringUTFChars(env, token, 0);
    }
    const char *cSession = NULL;
    if (session != NULL) {
        cSession = (*env)->GetStringUTFChars(env, session, 0);
    }

    /* user custom callback */
    xqc_cli_user_data_params_t *user_cfg = malloc(sizeof(xqc_cli_user_data_params_t));

    /* key param */
    user_cfg->url = cUrl;
    user_cfg->content = cContent;

    /* optional param */
    user_cfg->token = cToken;
    user_cfg->session = cSession;
    user_cfg->conn_timeout = time_out;
    user_cfg->max_recv_data_len = max_recv_data_len;

    /* headers */
    user_cfg->h3_hdrs.headers = headers;
    user_cfg->h3_hdrs.count = headersSize;

    switch (cc_type) {
        case 0:
            user_cfg->cc = CC_TYPE_BBR;
            break;
        case 1:
            user_cfg->cc = CC_TYPE_CUBIC;
            break;
        case 2:
            user_cfg->cc = CC_TYPE_RENO;
            break;
        default:
            user_cfg->cc = CC_TYPE_BBR;
    }

    /* callback */
    user_cfg->user_data_callback.env_android = env;
    user_cfg->user_data_callback.object_android = callback;
    user_cfg->user_data_callback.callback_read_data = callback_read_data;
    user_cfg->user_data_callback.callback_token = callback_token;
    user_cfg->user_data_callback.callback_session = callback_session;
    user_cfg->user_data_callback.callback_pt = callback_tp;

    return user_cfg;
}


/**
* 发送数据
 */
JNIEXPORT jint JNICALL Java_com_lizhi_component_net_xquic_native_XquicShortNative_send
        (JNIEnv *env, jclass cls, jobject param, jobject callback) {
    xqc_cli_user_data_params_t *user_param = get_data_params(env, param, callback);
    if (user_param == NULL) {
        return -1;
    }
    /* start to send data */
    client_send(user_param);
    return 0;
}