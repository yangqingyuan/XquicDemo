package com.lizhi.component.net.xquic.impl

import com.lizhi.component.net.xquic.XquicClient
import com.lizhi.component.net.xquic.listener.XCall
import com.lizhi.component.net.xquic.listener.XCallBack
import com.lizhi.component.net.xquic.mode.XRequest
import com.lizhi.component.net.xquic.mode.XResponse
import com.lizhi.component.net.xquic.utils.XLogUtils
import java.lang.Exception
import kotlin.properties.Delegates

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
) : XAsyncCallCommon(xCall, xquicClient, originalRequest, responseCallback) {

    companion object {
        const val TAG = "XAsyncCallReuse"
    }

    var connection: XConnection? = null
    private var startTime by Delegates.notNull<Long>()

    override fun execute() {
        startTime = System.currentTimeMillis()
        executed = true
        try {
            XLogUtils.debug("=======> execute start index(${indexTag})<========")
            val url = originalRequest.url.getHostUrl(xquicClient.dns)
            if (url.isNullOrBlank()) {
                responseCallback?.onFailure(
                    xCall,
                    Exception("dns can not parse domain ${originalRequest.url.url} error")
                )
                return
            }
            XLogUtils.debug(" url $url ")

            synchronized(TAG) {
                connection = xquicClient.connectionPool().get(originalRequest)
                if (connection == null) {
                    connection = XConnection(xquicClient, originalRequest, xCall)
                    xquicClient.connectionPool().put(connection!!) //add to pool
                }
            }

            //注意：这里使用index作为tag
            connection!!.send(
                indexTag.toString(),
                originalRequest.body?.content,/*responseCallback*/
                object : XCallBack {
                    override fun onFailure(call: XCall, exception: Exception) {
                        responseCallback?.onFailure(xCall, exception)
                        finish()
                    }

                    override fun onResponse(call: XCall, xResponse: XResponse) {
                        responseCallback?.onResponse(xCall, xResponse)
                        finish()
                    }
                })
        } catch (e: Exception) {
            e.printStackTrace()
            XLogUtils.error(e)
            cancel()
            finish()
        }
    }

    fun finish() {
        XLogUtils.debug("=======> execute end cost(${System.currentTimeMillis() - startTime} ms),index(${indexTag})<========")
        handle.removeMessages(indexTag)
        isFinish = true
        xquicClient.dispatcher().finished(this@XAsyncCallReuse)
    }

    override fun cancel() {
        super.cancel()
        connection?.cancel(indexTag.toString())
    }
}