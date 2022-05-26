package com.lizhi.component.net.xquic.impl

import com.lizhi.component.net.xquic.XquicClient
import com.lizhi.component.net.xquic.listener.XCall
import com.lizhi.component.net.xquic.listener.XCallBack
import com.lizhi.component.net.xquic.mode.XRequest
import com.lizhi.component.net.xquic.native.SendParams
import com.lizhi.component.net.xquic.utils.XLogUtils
import java.lang.Exception

/**
 * 作用: 复用调用
 * 作者: yqy
 * 创建日期: 2022/4/1.
 */
class XAsyncCallReuse(
    private var xCall: XCall,
    private var xquicClient: XquicClient,
    private var originalRequest: XRequest,
    private var responseCallback: XCallBack? = null,
    private var connection: XConnection? = null
) : XAsyncCallCommon(xCall, xquicClient, originalRequest, responseCallback) {

    override fun execute() {
        val startTime = System.currentTimeMillis()
        delayTime = startTime - createTime
        executed = true
        try {
            XLogUtils.debug("=======> execute start indexAA(${index})<========")
            val url = originalRequest.url.getHostUrl(xquicClient.dns)
            if (url.isNullOrBlank()) {
                responseCallback?.onFailure(
                    xCall,
                    Exception("dns can not parse domain ${originalRequest.url.url} error")
                )
                return
            }
            XLogUtils.debug(" url $url ")

            val sendParamsBuilder = SendParams.Builder()
                .setUrl(url)
                .setToken(XRttInfoCache.tokenMap[authority()])
                .setSession(XRttInfoCache.sessionMap[authority()])
                .setConnectTimeOut(xquicClient.connectTimeOut)
                .setReadTimeOut(xquicClient.readTimeout)
                .setMaxRecvLenght(1024 * 1024)
                .setCCType(xquicClient.ccType)

            sendParamsBuilder.setHeaders(parseHttpHeads())

            originalRequest.body?.let {
                sendParamsBuilder.setContent(it.content)
            }

            if (connection == null) {
                connection = XConnection()
                XConnectionPool.put(connection!!)
            }
            connection?.send(sendParamsBuilder.build())

            handle.removeMessages(index)
        } catch (e: Exception) {
            e.printStackTrace()
            XLogUtils.error(e)
            cancel()
        } finally {
            XLogUtils.debug("=======> execute end cost(${System.currentTimeMillis() - startTime} ms),index(${index})<========")
            isFinish = true
            xquicClient.dispatcher().finished(this)
        }
    }
}