package com.lizhi.component.net.xquic.impl

import com.lizhi.component.net.xquic.XquicClient
import com.lizhi.component.net.xquic.listener.XCall
import com.lizhi.component.net.xquic.listener.XCallBack
import com.lizhi.component.net.xquic.listener.XWebSocket
import com.lizhi.component.net.xquic.listener.XWebSocketListener
import com.lizhi.component.net.xquic.mode.XRequest
import com.lizhi.component.net.xquic.mode.XResponse
import com.lizhi.component.net.xquic.utils.XLogUtils
import java.lang.Exception
import java.util.ArrayDeque

/**
 * 作用: 链接
 * 作者: yqy
 * 创建日期: 2022/5/26.
 */
class XConnection(xquicClient: XquicClient, val xRequest: XRequest, val xCall: XCall) {

    companion object {
        private const val TAG = "XConnection"
    }

    private val authority = xRequest.url.authority
    private var xWebSocket: XWebSocket? = null

    private var xCallBackMap: MutableMap<String, XCallBack?> = mutableMapOf()

    var idleAtNanos = Long.MAX_VALUE
    var isDestroy = false

    /**
     * 存储还未链接的数据
     */
    private val messageQueue = ArrayDeque<XRealWebSocket.Message>()

    init {
        xquicClient.newWebSocket(xRequest, object : XWebSocketListener {
            override fun onOpen(webSocket: XWebSocket, response: XResponse) {
                XLogUtils.debug(TAG, "onOpen")
                xWebSocket = webSocket
                while (true) {
                    val content = messageQueue.poll() ?: return
                    webSocket.send(content)
                }
            }

            override fun onMessage(webSocket: XWebSocket, response: XResponse) {
                synchronized(this) {
                    XLogUtils.error(TAG, "onMessage")
                    xCallBackMap[response.xResponseBody.tag]?.onResponse(xCall, response)
                    xCallBackMap.remove(response.xResponseBody.tag)
                }
            }

            override fun onClosed(webSocket: XWebSocket, code: Int, reason: String?) {
                xWebSocket = null
                isDestroy = true
                XLogUtils.debug(TAG, "onClosed")
            }

            override fun onFailure(webSocket: XWebSocket, t: Throwable, response: XResponse) {
                synchronized(this) {
                    xWebSocket = null
                    isDestroy = true
                    XLogUtils.debug(TAG, "onFailure")
                    xCallBackMap.forEach(action = {
                        it.value?.onFailure(xCall, Exception(t))
                    })
                    xCallBackMap.clear()
                }
            }
        })
    }

    @Synchronized
    fun send(
        tag: String, content: String?, xCallBack: XCallBack?
    ) {
        if (isDestroy) {
            xCallBack?.onFailure(xCall, Exception("connection is destroy"))
            return
        }
        xCallBackMap[tag] = xCallBack
        val message = XRealWebSocket.Message(
            XRealWebSocket.Message.MSG_TYPE_SEND,
            content ?: "test",
            tag
        )

        if (xWebSocket != null) {
            xWebSocket?.send(message)
        } else {//如果没有链接，开始链接,并将参数缓存起来，握手成功后再发送信息
            messageQueue.add(
                message
            )
        }
    }


    /**
     * 是否符合条件
     */
    fun isEligible(xRequest: XRequest): Boolean {
        if (authority == xRequest.url.authority) {//通过域名来判断是否符合
            return true
        }
        return false
    }

    /**
     * 取消的时候移除监听
     */
    fun cancel(tag: String) {
        synchronized(this) {
            xCallBackMap.remove(tag)
        }
    }

    fun close() {
        xWebSocket?.cancel()
    }
}