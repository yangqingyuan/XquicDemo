//
// Created by lizhi on 2022/3/28.
//
#include "xquic_common.h"
#include "xquic_client_short.h"
#include "xquic_engine_callbacks.h"
#include "xquic_transport_callbacks.h"
#include "xquic_socket.h"
#include "xquic_h3_callbacks.h"
#include "xquic_h3_ctrl.h"


/**
 * 打开log文件
 * @param ctx
 * @return
 */
int client_long_open_log_file(xqc_cli_ctx_t *ctx) {
    ctx->log_fd = open(ctx->log_path, (O_WRONLY | O_APPEND | O_CREAT), 0644);
    if (ctx->log_fd <= 0) {
        return XQC_ERROR;
    }
    return XQC_OK;
}

/**
 * 关闭log文件
 * @param ctx
 */
int client_long_close_log_file(xqc_cli_ctx_t *ctx) {
    if (ctx->log_fd <= 0) {
        return XQC_ERROR;
    }

    close(ctx->log_fd);
    ctx->log_fd = 0;
    return XQC_OK;
}

/**
 * 打开关键log文件
 * @param ctx
 */
int client_long_open_keylog_file(xqc_cli_ctx_t *ctx) {
    ctx->keylog_fd = open(ctx->args->env_cfg.key_out_path, (O_WRONLY | O_APPEND | O_CREAT), 0644);
    if (ctx->keylog_fd <= 0) {
        return XQC_ERROR;
    }
    return XQC_OK;
}

/**
 * 关闭log文件
 * @param ctx
 */
int client_long_close_keylog_file(xqc_cli_ctx_t *ctx) {
    if (ctx->keylog_fd <= 0) {
        return XQC_ERROR;
    }

    close(ctx->keylog_fd);
    ctx->keylog_fd = 0;
    return XQC_OK;
}


/**
 * 初始化ctx
 * @param ctx
 * @param args
 */
void client_long_init_ctx(xqc_cli_ctx_t *pctx, xqc_cli_client_args_t *args) {
    DEBUG;
    strncpy(pctx->log_path, args->env_cfg.log_path, sizeof(pctx->log_path) - 1);
    pctx->args = args;
    client_long_open_log_file(pctx);
    client_long_open_keylog_file(pctx);
}

/**
 * 客户端引擎回调
 * @param main_loop
 * @param io_w
 * @param what
 */
void client_long_engine_callback(struct ev_loop *main_loop, ev_timer *io_w, int what) {
    //DEBUG;
    xqc_cli_ctx_t *ctx = (xqc_cli_ctx_t *) io_w->data;
    if (ctx && ctx->engine) {
        xqc_engine_main_logic(ctx->engine);
    }
}

/**
 * 引擎初始化SSL配置
 */
void client_long_init_engine_ssl_config(xqc_engine_ssl_config_t *cfg, xqc_cli_client_args_t *args) {
    memset(cfg, 0, sizeof(xqc_engine_ssl_config_t));
    if (args->quic_cfg.cipher_suites) {
        cfg->ciphers = args->quic_cfg.cipher_suites;
    } else {
        cfg->ciphers = XQC_TLS_CIPHERS;
    }
    cfg->groups = XQC_INTEROP_TLS_GROUPS;
}

/**
 * 初始化引擎回调
 * @param cb
 * @param transport_cbs
 * @param arg
 */
void client_long_init_engine_callback(xqc_engine_callback_t *cb,
                                      xqc_transport_callbacks_t *transport_cbs,
                                      xqc_cli_client_args_t *arg) {
    static xqc_engine_callback_t callback = {
            .log_callbacks = {
                    .xqc_log_write_err = client_write_log,
                    .xqc_log_write_stat = client_write_log
            },
            .keylog_cb = client_keylog_cb,
            .set_event_timer =client_set_event_timer
    };

    static xqc_transport_callbacks_t tcb = {
            .write_socket = client_write_socket,
            .save_token = client_save_token,
            .save_session_cb = client_save_session_cb,
            .save_tp_cb = client_save_tp_cb,
            .cert_verify_cb = client_cert_verify_cb,
            .conn_update_cid_notify = client_conn_update_cid_notify
    };

    *cb = callback;
    *transport_cbs = tcb;
}

/**
 * 初始化协议
 * 注意：这里只初始化H3，hq不做过多的逻辑
 * @param ctx
 * @return
 */
int client_long_init_alpn(xqc_cli_ctx_t *ctx) {
    xqc_h3_callbacks_t h3_cbs = {
            .h3c_cbs={
                    .h3_conn_create_notify = client_h3_conn_create_notify,
                    .h3_conn_close_notify = client_h3_conn_close_notify,
                    .h3_conn_handshake_finished = client_h3_conn_handshake_finished,
                    .h3_conn_ping_acked =client_h3_conn_ping_acked_notify,
            },
            .h3r_cbs={
                    .h3_request_create_notify = client_h3_request_create_notify,
                    .h3_request_close_notify = client_h3_request_close_notify,
                    .h3_request_read_notify = client_h3_request_read_notify,
                    .h3_request_write_notify = client_h3_request_write_notify,
            }
    };

    int ret = xqc_h3_ctx_init(ctx->engine, &h3_cbs);
    if (ret != XQC_OK) {
        LOGE("init h3 context error, ret:%d", ret);
        return XQC_ERROR;
    }
    LOGD("client init alpn success");
    return XQC_OK;
}

/**
 * 初始化引擎
 * @param ctx
 * @param args
 * @return
 */
int client_long_init_engine(xqc_cli_ctx_t *ctx, xqc_cli_client_args_t *args) {
    DEBUG;
    /* init engine ssl config*/
    xqc_engine_ssl_config_t engine_ssl_config;
    client_long_init_engine_ssl_config(&engine_ssl_config, args);

    /* init engine callback*/
    xqc_transport_callbacks_t transport_cbs;
    xqc_engine_callback_t callback;
    client_long_init_engine_callback(&callback, &transport_cbs, args);

    xqc_config_t config;
    if (xqc_engine_get_default_config(&config, XQC_ENGINE_CLIENT) < 0) {
        return XQC_ERROR;
    }
    config.cfg_log_level = args->env_cfg.log_level;
    config.cfg_log_event = 10;//open log
    config.cfg_log_level_name = args->env_cfg.log_level;//open log

    ctx->engine = xqc_engine_create(XQC_ENGINE_CLIENT, &config,
                                    &engine_ssl_config, &callback, &transport_cbs, ctx);

    if (ctx->engine == NULL) {
        LOGE("xqc_engine_create error");
        return XQC_ERROR;
    }

    /* init alpn (初始化协议)*/
    if (client_long_init_alpn(ctx) < 0) {
        LOGE("init alpn error");
        return XQC_ERROR;
    }

    return XQC_OK;
}


/**
 * 是否资源
 * @param ctx
 */
void client_long_free_ctx(xqc_cli_ctx_t *ctx) {
    client_long_close_keylog_file(ctx);
    client_long_close_log_file(ctx);
    if (ctx->args) {
        if (ctx->args->user_stream.send_body != NULL) {
            free(ctx->args->user_stream.send_body);
        }

        if (ctx->args->user_stream.recv_body != NULL) {
            free(ctx->args->user_stream.recv_body);
        }

        if (ctx->args->user_callback != NULL) {
            free(ctx->args->user_callback->h3_hdrs.headers);
            free(ctx->args->user_callback);
        }

        free(ctx->args);
        ctx->args = NULL;
    }


    if (ctx->task_ctx.tasks) {
        free(ctx->task_ctx.tasks);
        ctx->task_ctx.tasks = NULL;
    }

    if (ctx->task_ctx.schedule.schedule_info != NULL) {
        free(ctx->task_ctx.schedule.schedule_info);
        ctx->task_ctx.schedule.schedule_info = NULL;
    }
    ctx->engine = NULL;
    free(ctx);
    ctx = NULL;
}


/**
 * 初始化 单链接多流发送多请求
 * @param ctx
 * @param args
 */
void client_long_init_tasks_scmr(xqc_cli_task_ctx_t *tctx, xqc_cli_client_args_t *args) {
    tctx->task_cnt = 1;/* one task, one connection, all requests */

    /* init task list*/
    tctx->tasks = calloc(1, sizeof(xqc_cli_task_t) * 1);

    /*请求总数*/
    //tctx->tasks->req_cnt = args->req_cfg.request_cnt;
    tctx->tasks->reqs = args->req_cfg.reqs;

    /*init schedule 初始化调度器*/
    tctx->schedule.schedule_info = calloc(1, sizeof(xqc_cli_task_schedule_info_t) * 1);
}

/**
 * 初始化串/并行请求
 * @param ctx
 * @param args
 */
void client_long_init_tasks_scsr(xqc_cli_task_ctx_t *tctx, xqc_cli_client_args_t *args) {
    tctx->task_cnt = args->req_cfg.request_cnt;

    /*init task list */
    tctx->tasks = calloc(1, sizeof(xqc_cli_task_t) * tctx->task_cnt);
    for (int i = 0; i < tctx->task_cnt; i++) {
        tctx->tasks[i].task_idx = i;
        //tctx->tasks[i].req_cnt = 1;
        tctx->tasks[i].reqs = (xqc_cli_request_t *) args->req_cfg.reqs + i;
    }

    /*init schedule 初始化调度器*/
    tctx->schedule.schedule_info = calloc(1, sizeof(xqc_cli_task_schedule_info_t) * tctx->task_cnt);
}

/**
 * 初始化任务
 * @param ctx
 */
void client_long_init_tasks(xqc_cli_ctx_t *ctx) {

    /*任务模式*/
    ctx->task_ctx.mode = ctx->args->net_cfg.mode;
    switch (ctx->args->net_cfg.mode) {
        case MODE_SCMR://单链接多流发送多请求
            client_long_init_tasks_scmr(&ctx->task_ctx, ctx->args);
            break;
        case MODE_SCSR_SERIAL://串行：一个请求一个链接
        case MODE_SCSR_CONCURRENT://并行：一个请求一个链接
            client_long_init_tasks_scsr(&ctx->task_ctx, ctx->args);
            break;
        default:
            LOGE("init tasks error,unKnow mode");
            break;
    }
}


/**
 * 关闭任务
 * @param ctx
 * @param task
 */
int client_long_close_task(xqc_cli_ctx_t *ctx, xqc_cli_task_t *task) {
    DEBUG;
    xqc_cli_user_conn_t *user_conn = task->user_conn;

    if (!user_conn) {
        LOGW("is closed task,no need to close again!");
        return -1;
    }

    /* to free jni object */
    callback_msg_to_client(ctx->args, MSG_TYPE_DESTROY, NULL, 0);

    /* remove task event handle */
    ev_io_stop(ctx->eb, &user_conn->ev_socket);
    ev_timer_stop(ctx->eb, &user_conn->ev_timeout);

    /* close socket */
    if (user_conn->fd > -1) {
        close(user_conn->fd);
        user_conn->fd = -1;
    }

    /* free user_callback */
    if (ctx->args->user_callback != NULL) {
        free(ctx->args->user_callback->h3_hdrs.headers);
        free(ctx->args->user_callback);
        ctx->args->user_callback = NULL;
    }

    free(user_conn);
    user_conn = NULL;

    LOGI(">>>>>>>> free data success <<<<<<<<<");
    return 0;
}

/**
 * socket 事件回调
 * @param main_loop
 * @param io_w
 * @param what
 */
void client_long_socket_event_callback(struct ev_loop *main_loop, ev_io *io_w, int what) {
    //DEBUG;
    xqc_cli_user_conn_t *user_conn = (xqc_cli_user_conn_t *) io_w->data;
    if (what & EV_READ) {
        client_socket_read_handler(user_conn);
    } else if (what & EV_WRITE) {
        client_socket_write_handler(user_conn);
    } else {
        LOGE("socket event callback error,unKnow what:%d", what);
    }
}


/**
 * socket 超时后关闭流
 * @param main_loop
 * @param io_t
 * @param what
 */
void client_long_idle_callback(struct ev_loop *main_loop, ev_timer *io_t, int what) {
    DEBUG;
    xqc_cli_user_conn_t *user_conn = (xqc_cli_user_conn_t *) io_t->data;

    if (xqc_now() - user_conn->last_sock_read_time >=
        (uint64_t) user_conn->ctx->args->net_cfg.conn_timeout * 1000000) {
        int rc = 0;
        if (user_conn->ctx->args->quic_cfg.alpn_type == ALPN_H3) {
            rc = xqc_h3_conn_close(user_conn->ctx->engine, &user_conn->cid);
        } else {
            LOGE("不支持其他协议");
        }

        if (rc != XQC_OK) {
            LOGW("client idle callback,close conn error");
            //return;
        }

        LOGW("socket idle timeout(%ds), task failed, total task_cnt: %d, req_fin_cnt: %d, req_sent_cnt: %d, req_create_cnt: %d\n",
             user_conn->ctx->args->net_cfg.conn_timeout,
             user_conn->ctx->task_ctx.tasks[user_conn->task->task_idx].req_cnt,
             user_conn->ctx->task_ctx.schedule.schedule_info[user_conn->task->task_idx].req_fin_cnt,
             user_conn->ctx->task_ctx.schedule.schedule_info[user_conn->task->task_idx].req_sent_cnt,
             user_conn->ctx->task_ctx.schedule.schedule_info[user_conn->task->task_idx].req_create_cnt);

        //修改为失败状态
        user_conn->ctx->task_ctx.schedule.schedule_info[user_conn->task->task_idx].status = TASK_STATUS_FAILED;

        /* call back to client */
        char err_msg[214];
        sprintf(err_msg, "socket idle timeout(%ds)", user_conn->ctx->args->net_cfg.conn_timeout);
        callback_data_to_client(user_conn, XQC_ERROR, err_msg, NULL);
    }
}

/**
 * 初始化0rtt
 *
 * @param args
 */
void client_long_init_0rtt(xqc_cli_client_args_t *args) {
    /* read session ticket */
    DEBUG;
}

/**
 * 初始化链接设置
 * @param args
 */
void
client_long_init_connection_settings(xqc_conn_settings_t *settings, xqc_cli_client_args_t *args) {

    /* 拥塞控制*/
    xqc_cong_ctrl_callback_t cong_ctrl;
    switch (args->net_cfg.cc) {
        case CC_TYPE_BBR:
            cong_ctrl = xqc_bbr_cb;
            LOGD("cong_ctrl type xqc_bbr_cb");
            break;
        case CC_TYPE_CUBIC:
            cong_ctrl = xqc_cubic_cb;
            LOGD("cong_ctrl type xqc_cubic_cb");
            break;
        case CC_TYPE_RENO:
            cong_ctrl = xqc_reno_cb;
            LOGD("cong_ctrl type xqc_reno_cb");
            break;
        default:
            break;
    }

    memset(settings, 0, sizeof(xqc_conn_settings_t));
    settings->pacing_on = args->net_cfg.pacing;
    settings->ping_on = 1;
    settings->cong_ctrl_callback = cong_ctrl;//拥塞控制算法
    settings->cc_params.customize_on = 1;//是否打开自定义
    settings->cc_params.init_cwnd = 32;//拥塞窗口数
    settings->so_sndbuf = 1024 * 1024;//socket send  buf的大小
    settings->proto_version = XQC_VERSION_V1;
    settings->init_idle_time_out = (args->net_cfg.conn_timeout) * 1000;//xquic default 10s
    settings->idle_time_out = (args->net_cfg.read_timeout) * 1000;//xquic default
    settings->spurious_loss_detect_on = 1;//散列丢失检测
    settings->keyupdate_pkt_threshold = args->quic_cfg.keyupdate_pkt_threshold;//单个 1-rtt 密钥的数据包限制，0 表示无限制
}

/**
 * 初始化ssl配置
 * @param args
 */
void client_long_init_connection_ssl_config(xqc_conn_ssl_config_t *conn_ssl_config,
                                            xqc_cli_client_args_t *args) {
    memset(conn_ssl_config, 0, sizeof(xqc_conn_ssl_config_t));

    /*set session ticket and transport parameter args */
    if (args->quic_cfg.st_len < 0 || args->quic_cfg.tp_len < 0) {
        conn_ssl_config->session_ticket_data = NULL;
        conn_ssl_config->transport_parameter_data = NULL;
    } else {
        conn_ssl_config->session_ticket_data = args->quic_cfg.st;
        conn_ssl_config->session_ticket_len = args->quic_cfg.st_len;
        conn_ssl_config->transport_parameter_data = args->quic_cfg.tp;
        conn_ssl_config->transport_parameter_data_len = args->quic_cfg.tp_len;
    }

}

/**
 * 初始化链接
 * @return
 */
int client_long_init_connection(xqc_cli_user_conn_t *user_conn, xqc_cli_client_args_t *args) {
    DEBUG;
    client_long_init_0rtt(args);

    /* init connection settings */
    xqc_conn_settings_t conn_settings;
    client_long_init_connection_settings(&conn_settings, args);

    /* init config*/
    xqc_conn_ssl_config_t conn_ssl_config;
    client_long_init_connection_ssl_config(&conn_ssl_config, args);

    if (args->quic_cfg.alpn_type == ALPN_H3) {
        const xqc_cid_t *cid = xqc_h3_connect(user_conn->ctx->engine, &conn_settings,
                                              (const unsigned char *) args->quic_cfg.token,
                                              args->quic_cfg.token_len,
                                              args->net_cfg.host, 0, &conn_ssl_config,
                                              (struct sockaddr *) &args->net_cfg.addr,
                                              args->net_cfg.addr_len, user_conn);

        if (cid == NULL) {
            LOGE("xqc h3 connect error");
            return -1;
        }

        memcpy(&user_conn->cid, cid, sizeof(xqc_cid_t));
    } else {
        LOGE("只支持H3，暂时不支持其他类型");
        return -1;
    }
    return 0;
}


/**
 * 发送请求
 * @param user_conn
 * @param args
 * @param reqs
 * @param req_cnt
 */
void client_long_send_requests(xqc_cli_user_conn_t *user_conn, xqc_cli_client_args_t *args,
                               xqc_cli_request_t *reqs, queue_t *queue, int req_cut) {
    DEBUG;

    /* get data from queue to create one or more request */
    while (!queue_empty(queue)) {
        /* get first data */
        char *data = queue_front(queue);

        /* parse json data */
        cJSON *json_data = cJSON_Parse(data);
        char *content = cJSON_GetObjectItem(json_data, "send_body")->valuestring;
        char *tag = cJSON_GetObjectItem(json_data, "user_tag")->valuestring;
        cJSON *headers_json = cJSON_GetObjectItem(json_data, "headers");

        /* remove pop data */
        queue_pop(queue);

        /* new a stream */
        xqc_cli_user_stream_t *user_stream = calloc(1, sizeof(xqc_cli_user_stream_t));
        user_stream->user_conn = user_conn;

        /* copy tag */
        if (tag != NULL && strlen(tag) != 0) {
            memset(user_stream->user_tag, 0, 512);
            strcpy(user_stream->user_tag, tag);
        }

        /*copy send content */
        if (content != NULL) {
            size_t content_len = strlen(content);
            user_stream->send_body = malloc(content_len);
            strcpy(user_stream->send_body, content);//copy data
            user_stream->send_body_len = content_len;
        }

        /* format request */
        xqc_cli_request_t *request = NULL;
        if (headers_json != NULL) {
            if (req_cut > MAX_REQUEST_CNT) {
                req_cut = 0;
            }
            req_cut++;
            request = reqs + req_cut; //loop user request
            int size = cJSON_GetArraySize(headers_json);
            request->count = size;

            for (int i = 0; i < size; i++) {
                cJSON *item = cJSON_GetArrayItem(headers_json, i);

                char *name = cJSON_GetObjectItem(item, "name")->valuestring;
                char *value = cJSON_GetObjectItem(item, "value")->valuestring;
                int flags = cJSON_GetObjectItem(item, "flags")->valueint;

                xqc_http_header_t header = {
                        .name = {.iov_base = name, .iov_len = strlen(name)},
                        .value = {.iov_base = value, .iov_len = strlen(value)},
                        .flags = flags,
                };
                request->headers[i] = header;
            }
        }

        /*set recv body max len */
        if (args->user_callback->max_recv_data_len > 0) {
            user_stream->recv_body_max_len = args->user_callback->max_recv_data_len;
        } else {
            user_stream->recv_body_max_len = MAX_REC_DATA_LEN;
        }

        if (args->quic_cfg.alpn_type == ALPN_H3) {
            if (client_send_h3_requests(user_conn, user_stream, request) < 0) {
                char err_msg[214];
                sprintf(err_msg,
                        "xqc h3 request create error,please check network or retry,host=%s",
                        user_conn->ctx->args->net_cfg.host);
                LOGE("%s", err_msg);
                callback_data_to_client(user_conn, XQC_ERROR, err_msg, NULL);
                goto end;
            }
        } else {
            LOGE("只持者 h3发送");
        }
        user_conn->ctx->task_ctx.schedule.schedule_info[user_conn->task->task_idx].req_create_cnt++;

        end:
        /* free data */
        free(data);
        cJSON_Delete(json_data);
    }
}


/**
 * 处理任务
 * @param ctx
 * @param task
 * @return
 */
int client_long_handle_task(xqc_cli_ctx_t *ctx, xqc_cli_task_t *task) {
    DEBUG;

    /* create socket and connection callback user data*/
    xqc_cli_user_conn_t *user_conn = calloc(1, sizeof(xqc_cli_user_conn_t));
    user_conn->ctx = ctx;
    user_conn->task = task;
    user_conn->fd = client_create_socket(user_conn, &ctx->args->net_cfg);
    if (user_conn->fd < 0) {
        LOGE("client_create_socket error\n");
        return -1;
    }

    /*socket event*/
    user_conn->ev_socket.data = user_conn;
    ev_io_init(&user_conn->ev_socket, client_long_socket_event_callback, user_conn->fd,
               EV_READ);
    ev_io_start(ctx->eb, &user_conn->ev_socket);

    /*xquic timer */
    user_conn->ev_timeout.data = user_conn;
    ev_timer_init(&user_conn->ev_timeout, client_long_idle_callback,
                  ctx->args->net_cfg.conn_timeout,
                  0);
    ev_timer_start(ctx->eb, &user_conn->ev_timeout);

    task->user_conn = user_conn;

    /* init connect */
    if (XQC_OK != client_long_init_connection(user_conn, ctx->args)) {
        return -1;
    }
    return 0;
}

/**
 * 任务调度回调
 * @param main_loop
 * @param io_w
 * @param what
 */
void client_long_task_schedule_callback(struct ev_loop *main_loop, ev_async *io_w, int what) {
    //DEBUG;
    xqc_cli_ctx_t *ctx = (xqc_cli_ctx_t *) io_w->data;

    switch (ctx->msg_data.cmd_type) {
        case CMD_TYPE_NONE:
            break;
        case CMD_TYPE_INIT_TASK: {//init task data
            uint8_t idle_flag = 1;
            int idle_waiting_task_id = -1;

            for (int i = 0; i < ctx->task_ctx.task_cnt; i++) {
                /* record the first waiting task */
                if (idle_waiting_task_id == -1
                    && ctx->task_ctx.schedule.schedule_info[i].status == TASK_STATUS_WAITTING) {
                    idle_waiting_task_id = i;
                }
            }

            /* if dle and got a waiting task, run the task */
            if (idle_flag && idle_waiting_task_id >= 0) {
                /* handle task and set status to RUNNING */
                int ret = client_long_handle_task(ctx, ctx->task_ctx.tasks + idle_waiting_task_id);
                if (0 == ret) {
                    ctx->task_ctx.schedule.schedule_info[idle_waiting_task_id].status = TASK_STATUS_RUNNING;
                } else {
                    ctx->task_ctx.schedule.schedule_info[idle_waiting_task_id].status = TASK_STATUS_FAILED;
                }
            }
        }
            break;
        case CMD_TYPE_SEND_PING://send ping
            for (int i = 0; i < ctx->task_ctx.task_cnt; i++) {
                xqc_cli_task_t *task = ctx->task_ctx.tasks + i;
                client_send_H3_ping(task->user_conn, ctx->msg_data.ping_data);
            }
            break;
        case CMD_TYPE_SEND_DATA: //send data
            //LOGE("send data %s", ctx->msg_data.data);
            pthread_mutex_lock(ctx->mutex);
            //FIXME 如果是多链接的时候，永远只用第一个conn来进行请求，例如非 MODE_SCMR模式
            for (int i = 0; i < ctx->task_ctx.task_cnt; i++) {
                xqc_cli_task_t *task = ctx->task_ctx.tasks + i;
                int task_idx = task->task_idx;

                /* req_cun + queue size */
                ctx->task_ctx.tasks[task_idx].req_cnt += queue_size(&ctx->msg_data.queue);
                ctx->task_ctx.schedule.schedule_info[task_idx].fin_flag = 0;

                client_long_send_requests(task->user_conn, ctx->args, task->reqs,
                                          &ctx->msg_data.queue,
                                          ctx->task_ctx.tasks[task_idx].req_cnt);

            }
            pthread_mutex_unlock(ctx->mutex);
            break;
        case CMD_TYPE_CANCEL://cancel conn
            /* when timeout, close which not fin */
            for (int i = 0; i < ctx->task_ctx.task_cnt; i++) {
                xqc_cli_task_t *task = ctx->task_ctx.tasks + i;
                LOGW("auto close h3 conn,and wait to destroy");
                xqc_h3_conn_close(task->user_conn->ctx->engine, &task->user_conn->cid);
            }
            break;
        case CMD_TYPE_DESTROY:
            for (int i = 0; i < ctx->task_ctx.task_cnt; i++) {
                xqc_cli_task_t *task = ctx->task_ctx.tasks + i;
                client_long_close_task(ctx, task);
                ctx->task_ctx.schedule.schedule_info[i].fin_flag = 0;
            }
            LOGW("all tasks are finished,will break loop and exit!!");
            ev_break(main_loop, EVBREAK_ALL);
            break;
    }
    ctx->msg_data.cmd_type = CMD_TYPE_NONE;
}

/**
 * 生命时间过后，无论结果如何直接kill掉
 * @param main_loop
 * @param io_w
 * @param what
 */
void client_long_kill_it_any_way_callback(struct ev_loop *main_loop, ev_timer *io_w, int what) {
    DEBUG;
    ev_break(main_loop, EVBREAK_ALL);
}

/**
 * 开始任务管理器
 * @param ctx
 */
void client_long_start_task_manager(xqc_cli_ctx_t *ctx) {
    DEBUG;
    /*init tasks */
    client_long_init_tasks(ctx);

    /*init add arm task timer 这里轮询检查是否任务状态*/
    ctx->ev_task.data = ctx;
    ev_async_init(&ctx->ev_task, client_long_task_schedule_callback);
    ev_async_start(ctx->eb, &ctx->ev_task);

    /*init task */
    ctx->msg_data.cmd_type = CMD_TYPE_INIT_TASK;
    ev_async_send(ctx->eb, &ctx->ev_task);

    /* kill it anyway, to protect from endless task (如果设置了生命时长，并超时了生命时长，直接kill掉)*/
    if (ctx->args->env_cfg.life > 0) {
        ctx->ev_kill.data = ctx;
        ev_timer_init(&ctx->ev_kill, client_long_kill_it_any_way_callback, ctx->args->env_cfg.life,
                      0);
        ev_timer_start(ctx->eb, &ctx->ev_kill);
    }
}


/**
 * 初始化参数
 * （1）网络配置
 * （2）环境配置
 * （3）quic配置
 */
int client_long_init_args(xqc_cli_client_args_t *args, xqc_cli_user_data_params_t *user_param) {
    DEBUG;
    memset(args, 0, sizeof(xqc_cli_client_args_t));

    /*网络配置*/
    if (user_param->conn_timeout > 0) {
        args->net_cfg.conn_timeout = user_param->conn_timeout;
    } else {
        args->net_cfg.conn_timeout = 30;
    }
    args->net_cfg.mode = MODE_SCMR;
    args->net_cfg.cc = user_param->cc;
    args->net_cfg.conn_type = CONN_TYPE_LONG;

    args->req_cfg.request_cnt = 1;//TODO 这里默认一个url一个请求

    /*环境配置 */
    args->env_cfg.log_level = XQC_LOG_DEBUG;
    //strncpy(args->env_cfg.log_path,"xxxx",sizeof (args->env_cfg.log_path));
    //strncpy(args->env_cfg.out_file_dir,"xxxx",sizeof (args->env_cfg.out_file_dir));

    /*quic配置 */
    args->quic_cfg.alpn_type = ALPN_H3;
    strncpy(args->quic_cfg.alpn, "hq-interop", sizeof(args->quic_cfg.alpn));
    args->quic_cfg.keyupdate_pkt_threshold = UINT16_MAX;

    return 0;
}

/**
 * 解析参数
 * @param args
 * @param token
 * @param session
 * @param content
 */
int client_long_parse_args(xqc_cli_client_args_t *args, xqc_cli_user_data_params_t *user_param) {
    if (user_param->token != NULL) {
        size_t token_len = strlen(user_param->token);
        if (token_len < XQC_MAX_TOKEN_LEN) {
            strcpy(args->quic_cfg.token, user_param->token);//拷贝token
            args->quic_cfg.token_len = token_len;
        } else {
            LOGE("token set error : to lang > %d", XQC_MAX_TOKEN_LEN);
        }
    }
    if (user_param->session != NULL) {
        size_t session_len = strlen(user_param->session);
        if (session_len < MAX_SESSION_TICKET_LEN) {
            strcpy(args->quic_cfg.st, user_param->session);//拷贝session
            args->quic_cfg.st_len = session_len;
        } else {
            LOGE("session set error : to lang > %d", MAX_SESSION_TICKET_LEN);
        }
    }

    /* set callback */
    args->user_callback = user_param;

    /* parse server addr */
    int ret = client_parse_server_addr(&args->net_cfg, user_param->url,
                                       args->user_callback);//根据url解析地址跟port
    if (ret < 0) {
        free(args->user_stream.send_body);
        free(args->user_callback);
        free(args);
    }
    return ret;
}


/**
 * 链接
 * @param user_cfg
 * @return
 */
xqc_cli_ctx_t *client_long_conn(xqc_cli_user_data_params_t *user_param) {
    DEBUG;
    /*get input client args */
    xqc_cli_client_args_t *args = calloc(1, sizeof(xqc_cli_client_args_t));
    client_long_init_args(args, user_param);
    if (client_long_parse_args(args, user_param) < 0) {
        return NULL;
    }

    /*init client ctx*/
    xqc_cli_ctx_t *ctx = calloc(1, sizeof(xqc_cli_ctx_t));
    client_long_init_ctx(ctx, args);
    ctx->mutex = user_param->mutex;

    /* init queue */
    queue_init(&ctx->msg_data.queue);

    /* active */
    ctx->active = 1;
    return ctx;
}

/**
 * 开始
 * @param ctx
 * @return
 */
int client_long_start(xqc_cli_ctx_t *ctx) {
    DEBUG;
    if (ctx == NULL || ctx->active <= 0) {
        LOGE("client long start error: ctx = %p,active = %d", ctx, ctx->active);
        return -1;
    }
    /*engine event*/
    ctx->eb = ev_loop_new(EVFLAG_AUTO);
    ctx->ev_engine.data = ctx;
    ev_timer_init(&ctx->ev_engine, client_long_engine_callback, 0, 0);//EV_READ=1,EV_WRITE=2
    ev_timer_start(ctx->eb, &ctx->ev_engine);

    /* init engine */
    if (client_long_init_engine(ctx, ctx->args) != XQC_OK) {
        goto fail;
    }

    /* start task scheduler */
    client_long_start_task_manager(ctx);
    ev_run(ctx->eb, 0);

    fail:
    /* stop timer */
    ev_timer_stop(ctx->eb, &ctx->ev_engine);
    ev_async_stop(ctx->eb, &ctx->ev_task);
    ev_loop_destroy(ctx->eb);

    /* engine destroy */
    xqc_engine_destroy(ctx->engine);

    /* destroy queue */
    queue_destroy(&ctx->msg_data.queue);

    /* free ctx */
    client_long_free_ctx(ctx);

    return 0;
}

/**
 * 发送ping内容
 * @param ctx
 * @param ping_content
 * @return
 */
int client_long_send_ping(xqc_cli_ctx_t *ctx, char *ping_content) {
    //DEBUG;
    if (ctx == NULL || ctx->active <= 0) {
        LOGE("client long send ping error:  ctx = %p,active = %d", ctx, ctx->active);
        return -1;
    }
    ctx->msg_data.cmd_type = CMD_TYPE_SEND_PING;
    memset(ctx->msg_data.ping_data, 0, 256);
    strcpy(ctx->msg_data.ping_data, ping_content);
    ev_async_send(ctx->eb, &ctx->ev_task);
    return 0;
}

/**
 * H3的方式发送内容
 * @param ctx
 * @param content
 * @return
 */
int client_long_send(xqc_cli_ctx_t *ctx, const char *content) {
    DEBUG;

    if (ctx == NULL || ctx->active <= 0) {
        LOGE("client long send error:  ctx = %p,active = %d ", ctx, ctx->active);
        return -2;
    }

    size_t len = strlen(content);

    /* call method client_task_schedule_callback */
    ctx->msg_data.cmd_type = CMD_TYPE_SEND_DATA;
    char *data = malloc(len);
    memset(data, 0, len);
    strcpy(data, content);

    pthread_mutex_lock(ctx->mutex);

    /* push to queue */
    queue_push(&ctx->msg_data.queue, data);

    /* notify send */
    ev_async_send(ctx->eb, &ctx->ev_task);

    pthread_mutex_unlock(ctx->mutex);
    return 0;
}

/**
 * 取消
 * @return
 */
int client_long_cancel(xqc_cli_ctx_t *ctx) {
    DEBUG;
    if (ctx == NULL || ctx->active <= 0) {
        LOGE("client long cancel error: ctx = %p,active = %d", ctx, ctx->active);
        return -1;
    }
    /* call method client_task_schedule_callback */
    ctx->msg_data.cmd_type = CMD_TYPE_CANCEL;
    ev_async_send(ctx->eb, &ctx->ev_task);
    return 0;
}
