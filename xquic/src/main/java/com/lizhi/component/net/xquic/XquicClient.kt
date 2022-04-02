package com.lizhi.component.net.xquic

import com.lizhi.component.net.xquic.impl.XDispatcher
import com.lizhi.component.net.xquic.listener.XCall
import com.lizhi.component.net.xquic.impl.XRealCall
import com.lizhi.component.net.xquic.mode.XRequest

/**
 * 作用: 短链接
 * 作者: yqy
 * 创建日期: 2022/4/1.
 */
class XquicClient {

    /**
     *unit second
     */
    var connectTimeOut: Int = 0

    /**
     * unit second
     */
    var readTimeout: Int = 0

    /**
     * unit second
     */
    var writeTimeout: Int = 0

    /**
     * unit second
     */
    var pingInterval: Int = 0

    private val dispatcher by lazy { XDispatcher() }

    class Builder {
        val xquicClient = XquicClient()

        fun build(): XquicClient {
            return xquicClient
        }

        fun connectTimeOut(connectTimeout: Int): Builder {
            xquicClient.connectTimeOut = connectTimeout
            return this
        }

        fun setReadTimeOut(readTimeout: Int): Builder {
            xquicClient.readTimeout = readTimeout
            return this
        }

        fun writeTimeout(writeTimeout: Int): Builder {
            xquicClient.writeTimeout = writeTimeout
            return this
        }

        fun pingInterval(pingInterval: Int): Builder {
            xquicClient.pingInterval = pingInterval
            return this
        }
    }

    fun newCall(xRequest: XRequest): XCall {
        return XRealCall.newCall(this, xRequest)
    }

    fun dispatcher(): XDispatcher {
        return dispatcher
    }


}