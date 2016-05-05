/* mqtt.c
*  Protocol: http://docs.oasis-open.org/mqtt/mqtt/v3.1.1/os/mqtt-v3.1.1-os.html
*
* Copyright (c) 2014-2015, Tuan PM <tuanpm at live dot com>
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* * Redistributions of source code must retain the above copyright notice,
* this list of conditions and the following disclaimer.
* * Redistributions in binary form must reproduce the above copyright
* notice, this list of conditions and the following disclaimer in the
* documentation and/or other materials provided with the distribution.
* * Neither the name of Redis nor the names of its contributors may be used
* to endorse or promote products derived from this software without
* specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
* LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
* CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
* POSSIBILITY OF SUCH DAMAGE.
*/

#include "user_interface.h"
#include "osapi.h"
#include "espconn.h"
#include "os_type.h"
#include "mem.h"
#include "mqtt_msg.h"
#include "debug.h"
#include "user_config.h"
#include "config.h"
#include "mqtt.h"
#include "queue.h"

#define MQTT_TASK_PRIO        		0
#define MQTT_TASK_QUEUE_SIZE    	5  // 1
#define MQTT_SEND_TIMOUT			5

#ifndef QUEUE_BUFFER_SIZE
#define QUEUE_BUFFER_SIZE		 	2048
#endif

unsigned char *default_certificate;
unsigned int default_certificate_len = 0;
unsigned char *default_private_key;
unsigned int default_private_key_len = 0;

os_event_t mqtt_procTaskQueue[MQTT_TASK_QUEUE_SIZE];

LOCAL void ICACHE_FLASH_ATTR
mqtt_dns_found(const char *name, ip_addr_t *ipaddr, void *arg)
{
	struct espconn *pConn = (struct espconn *)arg;
	MQTT_Client* client = (MQTT_Client *)pConn->reverse;


	if(ipaddr == NULL)
	{
		INFO("DNS: Found, but got no ip, try to reconnect\r\n");
		client->connState = TCP_RECONNECT_REQ;
		return;
	}

	INFO("DNS: found ip %d.%d.%d.%d\n",
			*((uint8 *) &ipaddr->addr),
			*((uint8 *) &ipaddr->addr + 1),
			*((uint8 *) &ipaddr->addr + 2),
			*((uint8 *) &ipaddr->addr + 3));

	if(client->ip.addr == 0 && ipaddr->addr != 0)
	{
		os_memcpy(client->pCon->proto.tcp->remote_ip, &ipaddr->addr, 4);
		if(client->security){
			//os_printf("security flg\r\n");
			espconn_secure_connect(client->pCon);
		}
		else {
			espconn_connect(client->pCon);
		}

		client->connState = TCP_CONNECTING;
		INFO("TCP: connecting...\r\n");
	}

	system_os_post(MQTT_TASK_PRIO, 0, (os_param_t)client);
}



LOCAL void ICACHE_FLASH_ATTR
deliver_publish(MQTT_Client* client, uint8_t* message, int length)
{
	mqtt_event_data_t event_data;

	event_data.topic_length = length;
	event_data.topic = mqtt_get_publish_topic(message, &event_data.topic_length);
	event_data.data_length = length;
	event_data.data = mqtt_get_publish_data(message, &event_data.data_length);

	if(client->dataCb)
		client->dataCb((uint32_t*)client, event_data.topic, event_data.topic_length, event_data.data, event_data.data_length);

}


/**
  * @brief  Client received callback function.
  * @param  arg: contain the ip link information
  * @param  pdata: received data
  * @param  len: the lenght of received data
  * @retval None
  */
 #define COMBINE_CHECK 1
 #if COMBINE_CHECK
#define MQTT_PARSE_BUFFER_LEN 1800
#define MQTT_PARSE_DATA_TMP_TMP 512 //more than a single mqtt payload length
#define MQTT_DATA_LEN_MIN   2

 uint8 MQTT_PARSE_BUFFER[MQTT_PARSE_BUFFER_LEN] = {0};
 uint8 MQTT_PARSE_DATA_TMP[MQTT_PARSE_DATA_TMP_TMP] = {0};
//LOCAL uint8* pParse = MQTT_PARSE_BUFFER;
 uint32 parse_len = 0;
 BOOL parse_flag = false;
#endif

#if 0
//this function is to 
void ICACHE_FLASH_ATTR
    mqtt_copy_data_uncomplete(uint8* pdata,uint32 len )
{
	if(len<MQTT_PARSE_DATA_TMP_TMP){
		os_memcpy(MQTT_PARSE_DATA_TMP,pdata,len);
		parse_len = len;
		parse_flag = true;
		return;
	}else{
		INFO("COPY IMCOMPLETE PACKET ERROR\r\n");
		parse_len = 0;
		parse_flag = false;
		return;
	}

}
#endif


void ICACHE_FLASH_ATTR
mqtt_tcpclient_recv(void *arg, char *pdata, unsigned short len)
{
	uint8_t msg_type;
	uint8_t msg_qos;
	uint16_t msg_id;
	bool  multi_data = false;

	struct espconn *pCon = (struct espconn*)arg;
	MQTT_Client *client = (MQTT_Client *)pCon->reverse;
	

READPACKET:
	INFO("TCP: data received %d bytes\r\n", len);
	//uart1_tx_buffer(pdata,len);
#if COMBINE_CHECK
	if(parse_flag == true){
		if(parse_len+len <= MQTT_PARSE_BUFFER_LEN){
			os_printf("COMBINE PARSE DATA!!!!!!!!\r\n");
			os_memcpy(MQTT_PARSE_BUFFER,MQTT_PARSE_DATA_TMP,parse_len);
			os_memcpy(MQTT_PARSE_BUFFER+parse_len,pdata,len);
			pdata = MQTT_PARSE_BUFFER;
			len = parse_len+len ; 
			parse_flag = false;
			parse_len = 0;

		}else{
			parse_flag = false;
			parse_len = 0;
			INFO("MQTT: COMBINE PARSE DATA ERROR\r\n");
			return;
		}
	}
#endif
	//if(len < MQTT_BUF_SIZE && len > 0){
	if(len > 0){  //user pdata point directly , do not care about the buf length
		//mqtt_copy_data_uncomplete(pdata, len );
		//return;
#if COMBINE_CHECK
		if(len<MQTT_DATA_LEN_MIN){
			if(len<MQTT_PARSE_DATA_TMP_TMP){
				INFO("MQTT:  COPY IMPOMPLETE PACKET DATA\r\n");
				os_memcpy(MQTT_PARSE_DATA_TMP,pdata,len);
				parse_len = len;
				parse_flag = true;
				return;
			}else{
				INFO("MQTT: COPY IMCOMPLETE PACKET ERROR\r\n");
				parse_len = 0;
				parse_flag = false;
				return;
			}
		}
#endif
		//os_memcpy(client->mqtt_state.in_buffer, pdata, len);//do not copy , use pointer directly
		client->mqtt_state.in_buffer = pdata;
		client->mqtt_state.in_buffer_length = len;

		msg_type = mqtt_get_type(client->mqtt_state.in_buffer);
		msg_qos = mqtt_get_qos(client->mqtt_state.in_buffer);
		msg_id = mqtt_get_id(client->mqtt_state.in_buffer, client->mqtt_state.in_buffer_length);
		INFO("MQTT: type:%d ; qos : %d ; id : %d\r\n",msg_type,msg_qos,msg_id);
		switch(client->connState){
		case MQTT_CONNECT_SENDING:
			INFO("in case connect sending\r\n");
			if(msg_type == MQTT_MSG_TYPE_CONNACK){
				if(client->mqtt_state.pending_msg_type != MQTT_MSG_TYPE_CONNECT){
					INFO("MQTT: Invalid packet\r\n");
					if(client->security){
						os_printf("security flg\r\n");
						espconn_secure_disconnect(client->pCon);
					}
					else {
						espconn_disconnect(client->pCon);
					}
				} else {
					INFO("MQTT: Connected to %s:%d\r\n", client->host, client->port);
					client->connState = MQTT_DATA;
					if(client->connectedCb)
						client->connectedCb((uint32_t*)client);
				}

			}
			break;
		case MQTT_DATA:
			//INFO("in case mqtt data; len : %d\r\n",len);
			/*
			if(len<MQTT_DATA_LEN_MIN){
				INFO("Warning: Data too short\r\n");
				os_memcpy(MQTT_PARSE_BUFFER,pdata,len);
				parse_flag = true;
				parse_len = len;
				break;
				
			}
			*/
			
			client->mqtt_state.message_length_read = len;
			client->mqtt_state.message_length = mqtt_get_total_length(client->mqtt_state.in_buffer, client->mqtt_state.message_length_read);
			
			//INFO("TEST LENGTH:  len : %d ;  message_length: %d \r\n",client->mqtt_state.message_length_read,client->mqtt_state.message_length);
			 #if COMBINE_CHECK
			if(  ( (client->mqtt_state.message_length_read) < (client->mqtt_state.message_length))  || ((client->mqtt_state.message_length) == -1)){
				//mqtt_copy_data_uncomplete(pdata,len );
				//return;
				
				if(len<MQTT_PARSE_DATA_TMP_TMP){
					//INFO("COPY IMPOMPLETE PACKET DATA\r\n");
					os_printf("COPY IMPOMPLETE PACKET DATA\r\n");

					os_memcpy(MQTT_PARSE_DATA_TMP,pdata,len);
					parse_len = len;
					parse_flag = true;
					return;
				}else{
					//INFO("COPY IMCOMPLETE PACKET ERROR\r\n");
					os_printf("COPY IMCOMPLETE PACKET ERROR\r\n");
					parse_len = 0;
					parse_flag = false;
					return;
				}
			}
			#endif

			switch(msg_type)
			{

			  case MQTT_MSG_TYPE_SUBACK:
				if(client->mqtt_state.pending_msg_type == MQTT_MSG_TYPE_SUBSCRIBE && client->mqtt_state.pending_msg_id == msg_id)
				  INFO("MQTT: Subscribe successful\r\n");
				break;
			  case MQTT_MSG_TYPE_UNSUBACK:
				if(client->mqtt_state.pending_msg_type == MQTT_MSG_TYPE_UNSUBSCRIBE && client->mqtt_state.pending_msg_id == msg_id)
				  INFO("MQTT: UnSubscribe successful\r\n");
				break;
			  case MQTT_MSG_TYPE_PUBLISH:
				if(msg_qos == 1)
					client->mqtt_state.outbound_message = mqtt_msg_puback(&client->mqtt_state.mqtt_connection, msg_id);
				else if(msg_qos == 2)
					client->mqtt_state.outbound_message = mqtt_msg_pubrec(&client->mqtt_state.mqtt_connection, msg_id);
				if(msg_qos == 1 || msg_qos == 2){
					INFO("MQTT: Queue response QoS: %d\r\n", msg_qos);
					if(QUEUE_Puts(&client->msgQueue, client->mqtt_state.outbound_message->data, client->mqtt_state.outbound_message->length) == -1){
						INFO("MQTT: Queue full\r\n");
					}
				}

				deliver_publish(client, client->mqtt_state.in_buffer, client->mqtt_state.message_length_read);
				break;
			  case MQTT_MSG_TYPE_PUBACK:
				if(client->mqtt_state.pending_msg_type == MQTT_MSG_TYPE_PUBLISH && client->mqtt_state.pending_msg_id == msg_id){
				  INFO("MQTT: received MQTT_MSG_TYPE_PUBACK, finish QoS1 publish\r\n");
				}

				break;
			  case MQTT_MSG_TYPE_PUBREC:
				  client->mqtt_state.outbound_message = mqtt_msg_pubrel(&client->mqtt_state.mqtt_connection, msg_id);
				  if(QUEUE_Puts(&client->msgQueue, client->mqtt_state.outbound_message->data, client->mqtt_state.outbound_message->length) == -1){
				  	INFO("MQTT: Queue full\r\n");
				  }
				break;
			  case MQTT_MSG_TYPE_PUBREL:
				  client->mqtt_state.outbound_message = mqtt_msg_pubcomp(&client->mqtt_state.mqtt_connection, msg_id);
				  if(QUEUE_Puts(&client->msgQueue, client->mqtt_state.outbound_message->data, client->mqtt_state.outbound_message->length) == -1){
					INFO("MQTT: Queue full\r\n");
				  }
				break;
			  case MQTT_MSG_TYPE_PUBCOMP:
				if(client->mqtt_state.pending_msg_type == MQTT_MSG_TYPE_PUBLISH && client->mqtt_state.pending_msg_id == msg_id){
				  INFO("MQTT: receive MQTT_MSG_TYPE_PUBCOMP, finish QoS2 publish\r\n");
				}
				break;
			  case MQTT_MSG_TYPE_PINGREQ:
				  client->mqtt_state.outbound_message = mqtt_msg_pingresp(&client->mqtt_state.mqtt_connection);
				  if(QUEUE_Puts(&client->msgQueue, client->mqtt_state.outbound_message->data, client->mqtt_state.outbound_message->length) == -1){
					INFO("MQTT: Queue full\r\n");
				  }
				break;
			  case MQTT_MSG_TYPE_PINGRESP:
				// Ignore
				break;
			}
			// NOTE: this is done down here and not in the switch case above
			// because the PSOCK_READBUF_LEN() won't work inside a switch
			// statement due to the way protothreads resume.
			if(msg_type == MQTT_MSG_TYPE_PUBLISH)
			{
			  len = client->mqtt_state.message_length_read;

			  if(client->mqtt_state.message_length < client->mqtt_state.message_length_read)
			  {
				  //client->connState = MQTT_PUBLISH_RECV;
				  //Not Implement yet
				  len -= client->mqtt_state.message_length;
				  pdata += client->mqtt_state.message_length;

				  INFO("Get another published message\r\n");
				  goto READPACKET;
			  }

			}
			break;
		}
	} else {
		INFO("ERROR: Message too long:%d\r\n",len);
	}
	system_os_post(MQTT_TASK_PRIO, 0, (os_param_t)client);
}

/**
  * @brief  Client send over callback function.
  * @param  arg: contain the ip link information
  * @retval None
  */
void ICACHE_FLASH_ATTR
mqtt_tcpclient_sent_cb(void *arg)
{
	struct espconn *pCon = (struct espconn *)arg;
	MQTT_Client* client = (MQTT_Client *)pCon->reverse;
	INFO("TCP: Sent\r\n");
	client->sendTimeout = 0;
	if(client->connState == MQTT_DATA && client->mqtt_state.pending_msg_type == MQTT_MSG_TYPE_PUBLISH){
		if(client->publishedCb)
			client->publishedCb((uint32_t*)client);
	}
	system_os_post(MQTT_TASK_PRIO, 0, (os_param_t)client);
}

void ICACHE_FLASH_ATTR mqtt_timer(void *arg)
{
	TIMER_DBG("%s\r\n",__func__);

	MQTT_Client* client = (MQTT_Client*)arg;
    if(client){
		//INFO("MQTT:test in client.timer\r\n");
    	if(client->connState == MQTT_DATA){
    		client->keepAliveTick ++;
    		if(client->keepAliveTick > client->mqtt_state.connect_info->keepalive){
    
    			INFO("\r\nMQTT: Send keepalive packet to %s:%d!\r\n", client->host, client->port);
    			client->mqtt_state.outbound_message = mqtt_msg_pingreq(&client->mqtt_state.mqtt_connection);
    			client->mqtt_state.pending_msg_type = MQTT_MSG_TYPE_PINGREQ;
    			client->mqtt_state.pending_msg_type = mqtt_get_type(client->mqtt_state.outbound_message->data);
    			client->mqtt_state.pending_msg_id = mqtt_get_id(client->mqtt_state.outbound_message->data, client->mqtt_state.outbound_message->length);
    
    
    			client->sendTimeout = MQTT_SEND_TIMOUT;
    			INFO("MQTT: Sending, type: %d, id: %04X\r\n",client->mqtt_state.pending_msg_type, client->mqtt_state.pending_msg_id);
    			if(client->security){
    				os_printf("security flg\r\n");
    				espconn_secure_sent(client->pCon, client->mqtt_state.outbound_message->data, client->mqtt_state.outbound_message->length);
    			}
    			else{
    				espconn_sent(client->pCon, client->mqtt_state.outbound_message->data, client->mqtt_state.outbound_message->length);
    			}
    
    			client->mqtt_state.outbound_message = NULL;
    
    			client->keepAliveTick = 0;
    			system_os_post(MQTT_TASK_PRIO, 0, (os_param_t)client);
    		}
    
    	} else if(client->connState == TCP_RECONNECT_REQ){
    		client->reconnectTick ++;
    		if(client->reconnectTick > MQTT_RECONNECT_TIMEOUT) {
    			client->reconnectTick = 0;
    			client->connState = TCP_RECONNECT;
    			system_os_post(MQTT_TASK_PRIO, 0, (os_param_t)client);
    		}
    	}
    }else{
        INFO("MQTT: client empty in client.timer\r\n");
    }
	if(client->sendTimeout > 0)
		client->sendTimeout --;
}

void ICACHE_FLASH_ATTR
mqtt_tcpclient_discon_cb(void *arg)
{
    TIMER_DBG("%s\r\n",__func__);
	os_printf("mqtt_tcpclient_discon_cb\r\n");
	struct espconn *pespconn = (struct espconn *)arg;
	if(pespconn){
    	MQTT_Client* client = (MQTT_Client *)pespconn->reverse;
		if(client){
        	INFO("TCP: Disconnected callback\r\n");
        	client->connState = TCP_RECONNECT_REQ;
        	if(client->disconnectedCb){
				INFO("CALL DISCONNECT CB @ %p\r\n",client->disconnectedCb);
        		client->disconnectedCb((uint32_t*)client);
				system_os_post(MQTT_TASK_PRIO, 0, (os_param_t)client);
        	}
        
        	/*reset mqtt parse buffer*/
        	parse_flag = false;
        	parse_len = 0;
        	/*if tcp link break, drop unparsed data*/
        	//system_os_post(MQTT_TASK_PRIO, 0, (os_param_t)client);
		}else{
           os_printf("client already null in disconn cb\r\n");
		}
	}else{
        os_printf("pespconn already null in disconn cb\r\n");
	}
	
}



/**
  * @brief  Tcp client connect success callback function.
  * @param  arg: contain the ip link information
  * @retval None
  */
void ICACHE_FLASH_ATTR
mqtt_tcpclient_connect_cb(void *arg)
{
	struct espconn *pCon = (struct espconn *)arg;
	MQTT_Client* client = (MQTT_Client *)pCon->reverse;

	espconn_regist_disconcb(client->pCon, mqtt_tcpclient_discon_cb);
	espconn_regist_recvcb(client->pCon, mqtt_tcpclient_recv);////////
	espconn_regist_sentcb(client->pCon, mqtt_tcpclient_sent_cb);///////
	INFO("MQTT: Connected to broker %s:%d\r\n", client->host, client->port);

	mqtt_msg_init(&client->mqtt_state.mqtt_connection, client->mqtt_state.out_buffer, client->mqtt_state.out_buffer_length);
	client->mqtt_state.outbound_message = mqtt_msg_connect(&client->mqtt_state.mqtt_connection, client->mqtt_state.connect_info);
	client->mqtt_state.pending_msg_type = mqtt_get_type(client->mqtt_state.outbound_message->data);
	client->mqtt_state.pending_msg_id = mqtt_get_id(client->mqtt_state.outbound_message->data, client->mqtt_state.outbound_message->length);


	client->sendTimeout = MQTT_SEND_TIMOUT;
	INFO("MQTT: Sending, type: %d, id: %04X\r\n",client->mqtt_state.pending_msg_type, client->mqtt_state.pending_msg_id);
	if(client->security){
		os_printf("security flg\r\n");
		espconn_secure_sent(client->pCon, client->mqtt_state.outbound_message->data, client->mqtt_state.outbound_message->length);
	}
	else{
		espconn_sent(client->pCon, client->mqtt_state.outbound_message->data, client->mqtt_state.outbound_message->length);
	}

	client->mqtt_state.outbound_message = NULL;
	client->connState = MQTT_CONNECT_SENDING;
	system_os_post(MQTT_TASK_PRIO, 0, (os_param_t)client);
}

/**
  * @brief  Tcp client connect repeat callback function.
  * @param  arg: contain the ip link information
  * @retval None
  */
void ICACHE_FLASH_ATTR
mqtt_tcpclient_recon_cb(void *arg, sint8 errType)
{
    TIMER_DBG("%s\r\n",__func__);
	struct espconn *pCon = (struct espconn *)arg;
	if(pCon){
    	MQTT_Client* client = (MQTT_Client *)pCon->reverse;
        if(client){
        	INFO("TCP: Reconnect to %s:%d\r\n", client->host, client->port);
        	INFO("TCP_RECONNECT_REQ\r\n"); 
        	client->connState = TCP_RECONNECT_REQ;
        
        	/*reset mqtt parse buffer*/
        	parse_flag = false;
        	parse_len = 0;
        	/*if tcp link break, drop unparsed data*/
        	
        	system_os_post(MQTT_TASK_PRIO, 0, (os_param_t)client);
    	}else{
            os_printf("client null in recon cb\r\n");
		}
	}else{
        os_printf("already null in recon cb\r\n");
	}

}

/**
  * @brief  MQTT publish function.
  * @param  client: 	MQTT_Client reference
  * @param  topic: 		string topic will publish to
  * @param  data: 		buffer data send point to
  * @param  data_length: length of data
  * @param  qos:		qos
  * @param  retain:		retain
  * @retval TRUE if success queue
  */
BOOL ICACHE_FLASH_ATTR
MQTT_Publish(MQTT_Client *client, const char* topic, const char* data, int data_length, int qos, int retain)
{
	uint8_t dataBuffer[MQTT_BUF_SIZE];
	uint16_t dataLen;
	//INFO("------------------\r\n");
	//INFO("publish: \r\n");
	//INFO("topic : %s\r\n",topic);
	//INFO("data: %s \r\n",data);
	//INFO("==============\r\n");
	client->mqtt_state.outbound_message = mqtt_msg_publish(&client->mqtt_state.mqtt_connection,
										 topic, data, data_length,
										 qos, retain,
										 &client->mqtt_state.pending_msg_id);
	if(client->mqtt_state.outbound_message->length == 0){
		INFO("MQTT: Queuing publish failed\r\n");
		return FALSE;
	}
	INFO("MQTT: queuing publish, length: %d, queue size(%d/%d)\r\n", client->mqtt_state.outbound_message->length, client->msgQueue.rb.fill_cnt, client->msgQueue.rb.size);
	while(QUEUE_Puts(&client->msgQueue, client->mqtt_state.outbound_message->data, client->mqtt_state.outbound_message->length) == -1){
		INFO("MQTT: Queue full\r\n");
		if(QUEUE_Gets(&client->msgQueue, dataBuffer, &dataLen, MQTT_BUF_SIZE) == -1) {
			INFO("MQTT: Serious buffer error\r\n");
			return FALSE;
		}
	}
	system_os_post(MQTT_TASK_PRIO, 0, (os_param_t)client);
	return TRUE;
}

/**
  * @brief  MQTT subscibe function.
  * @param  client: 	MQTT_Client reference
  * @param  topic: 		string topic will subscribe
  * @param  qos:		qos
  * @retval TRUE if success queue
  */
BOOL ICACHE_FLASH_ATTR
MQTT_Subscribe(MQTT_Client *client, char* topic, uint8_t qos)
{
	uint8_t dataBuffer[MQTT_BUF_SIZE];
	uint16_t dataLen;

	client->mqtt_state.outbound_message = mqtt_msg_subscribe(&client->mqtt_state.mqtt_connection,
											topic, 0,
											&client->mqtt_state.pending_msg_id);
	INFO("MQTT: queue subscribe, topic\"%s\", id: %d\r\n",topic, client->mqtt_state.pending_msg_id);
	while(QUEUE_Puts(&client->msgQueue, client->mqtt_state.outbound_message->data, client->mqtt_state.outbound_message->length) == -1){
		INFO("MQTT: Queue full\r\n");
		if(QUEUE_Gets(&client->msgQueue, dataBuffer, &dataLen, MQTT_BUF_SIZE) == -1) {
			INFO("MQTT: Serious buffer error\r\n");
			return FALSE;
		}
	}
	system_os_post(MQTT_TASK_PRIO, 0, (os_param_t)client);
	return TRUE;
}




uint8 mqtt_last_status = 0;
#define MQTT_ALREADY_CONNECTED  0
#define MQTT_NOT_CONNECTED    1
void ICACHE_FLASH_ATTR
MQTT_Task(os_event_t *e)
{
	TIMER_DBG("%s\r\n",__func__);
	UART_WaitTxFifoEmpty(1);
	MQTT_Client* client = (MQTT_Client*)e->par;

	if(client == NULL || client->pCon==NULL) return;

	
	uint8_t dataBuffer[MQTT_BUF_SIZE];
	uint16_t dataLen;
	switch(client->connState){

	case TCP_RECONNECT_REQ:
		break;
	case TCP_RECONNECT:
		if(mqtt_last_status == MQTT_ALREADY_CONNECTED){
			if(client->disconnectedCb){
				client->disconnectedCb((uint32_t*)client);
			}
		}
		mqtt_last_status = MQTT_NOT_CONNECTED;
		MQTT_Connect(client);
		INFO("TCP: Reconnect to: %s:%d\r\n", client->host, client->port);
		INFO("TCP_RECONNECT\r\n"); 

		client->connState = TCP_CONNECTING;
		break;
	case MQTT_DATA:
		mqtt_last_status = MQTT_ALREADY_CONNECTED;
		if(QUEUE_IsEmpty(&client->msgQueue) || client->sendTimeout != 0) {
			break;
		}
		if(QUEUE_Gets(&client->msgQueue, dataBuffer, &dataLen, MQTT_BUF_SIZE) == 0){
			client->mqtt_state.pending_msg_type = mqtt_get_type(dataBuffer);
			client->mqtt_state.pending_msg_id = mqtt_get_id(dataBuffer, dataLen);


			client->sendTimeout = MQTT_SEND_TIMOUT;
			INFO("MQTT: Sending, type: %d, id: %04X\r\n",client->mqtt_state.pending_msg_type, client->mqtt_state.pending_msg_id);
			if(client->security){
				os_printf("security flg\r\n");
				espconn_secure_sent(client->pCon, dataBuffer, dataLen);
			}
			else{
				espconn_sent(client->pCon, dataBuffer, dataLen);
			}

			client->mqtt_state.outbound_message = NULL;
			break;
		}
		break;
	}
}

/**
  * @brief  MQTT initialization connection function
  * @param  client: 	MQTT_Client reference
  * @param  host: 	Domain or IP string
  * @param  port: 	Port to connect
  * @param  security:		1 for ssl, 0 for none
  * @retval None
  */
void ICACHE_FLASH_ATTR
MQTT_InitConnection(MQTT_Client *mqttClient, uint8_t* host, uint32 port, uint8_t security)
{
	uint32_t temp;
	INFO("MQTT_InitConnection\r\n");
	os_memset(mqttClient, 0, sizeof(MQTT_Client));
	temp = os_strlen(host);
	mqttClient->host = (uint8_t*)os_zalloc(temp + 1);
	os_strcpy(mqttClient->host, host);
	mqttClient->host[temp] = 0;
	mqttClient->port = port;
	mqttClient->security = security;

}

/**
  * @brief  MQTT initialization mqtt client function
  * @param  client: 	MQTT_Client reference
  * @param  clientid: 	MQTT client id
  * @param  client_user:MQTT client user
  * @param  client_pass:MQTT client password
  * @param  client_pass:MQTT keep alive timer, in second
  * @retval None
  */
//LOCAL uint8 MQTT_IN_BUF[MQTT_BUF_SIZE] = {0};
uint8 MQTT_OUT_BUF[MQTT_BUF_SIZE] = {0};
uint8 MQTT_QUEUE_BUF[QUEUE_BUFFER_SIZE] = {0};
void ICACHE_FLASH_ATTR
MQTT_InitClient(MQTT_Client *mqttClient, uint8_t* client_id, uint8_t* client_user, uint8_t* client_pass, uint32_t keepAliveTime, uint8_t cleanSession)
{
	uint32_t temp;
	INFO("MQTT_InitClient\r\n");

	os_memset(&mqttClient->connect_info, 0, sizeof(mqtt_connect_info_t));

	temp = os_strlen(client_id);
	mqttClient->connect_info.client_id = (uint8_t*)os_zalloc(temp + 1);
	os_strcpy(mqttClient->connect_info.client_id, client_id);
	mqttClient->connect_info.client_id[temp] = 0;

	temp = os_strlen(client_user);
	mqttClient->connect_info.username = (uint8_t*)os_zalloc(temp + 1);
	os_strcpy(mqttClient->connect_info.username, client_user);
	mqttClient->connect_info.username[temp] = 0;

	temp = os_strlen(client_pass);
	mqttClient->connect_info.password = (uint8_t*)os_zalloc(temp + 1);
	os_strcpy(mqttClient->connect_info.password, client_pass);
	mqttClient->connect_info.password[temp] = 0;


	mqttClient->connect_info.keepalive = keepAliveTime;
	mqttClient->connect_info.clean_session = cleanSession;

	//mqttClient->mqtt_state.in_buffer = (uint8_t *)os_zalloc(MQTT_BUF_SIZE);
	//mqttClient->mqtt_state.in_buffer = MQTT_IN_BUF;
	//mqttClient->mqtt_state.in_buffer_length = MQTT_BUF_SIZE;
	mqttClient->mqtt_state.in_buffer = NULL;
	mqttClient->mqtt_state.in_buffer_length = 0;
	
	//mqttClient->mqtt_state.out_buffer =  (uint8_t *)os_zalloc(MQTT_BUF_SIZE);
	mqttClient->mqtt_state.out_buffer = MQTT_OUT_BUF;
	mqttClient->mqtt_state.out_buffer_length = MQTT_BUF_SIZE;
	mqttClient->mqtt_state.connect_info = &mqttClient->connect_info;

	mqtt_msg_init(&mqttClient->mqtt_state.mqtt_connection, mqttClient->mqtt_state.out_buffer, mqttClient->mqtt_state.out_buffer_length);

	//QUEUE_Init(&mqttClient->msgQueue, QUEUE_BUFFER_SIZE);
	QUEUE_Init_Ptr(&mqttClient->msgQueue, MQTT_QUEUE_BUF,QUEUE_BUFFER_SIZE);

	system_os_task(MQTT_Task, MQTT_TASK_PRIO, mqtt_procTaskQueue, MQTT_TASK_QUEUE_SIZE);
	system_os_post(MQTT_TASK_PRIO, 0, (os_param_t)mqttClient);
}
void ICACHE_FLASH_ATTR
MQTT_InitLWT(MQTT_Client *mqttClient, uint8_t* will_topic, uint8_t* will_msg, uint8_t will_qos, uint8_t will_retain)
{
	uint32_t temp;
	temp = os_strlen(will_topic);
	mqttClient->connect_info.will_topic = (uint8_t*)os_zalloc(temp + 1);
	os_strcpy(mqttClient->connect_info.will_topic, will_topic);
	mqttClient->connect_info.will_topic[temp] = 0;

	temp = os_strlen(will_msg);
	mqttClient->connect_info.will_message = (uint8_t*)os_zalloc(temp + 1);
	os_strcpy(mqttClient->connect_info.will_message, will_msg);
	mqttClient->connect_info.will_message[temp] = 0;


	mqttClient->connect_info.will_qos = will_qos;
	mqttClient->connect_info.will_retain = will_retain;
}
/**
  * @brief  Begin connect to MQTT broker
  * @param  client: MQTT_Client reference
  * @retval None
  */

LOCAL struct espconn mqtt_conn;
LOCAL struct _esp_tcp mqtt_tcp;


void ICACHE_FLASH_ATTR
MQTT_Connect(MQTT_Client *mqttClient)
{
	MQTT_Disconnect(mqttClient);
	//mqttClient->pCon = (struct espconn *)os_zalloc(sizeof(struct espconn));
	mqttClient->pCon = &mqtt_conn;
	mqttClient->pCon->type = ESPCONN_TCP;
	mqttClient->pCon->state = ESPCONN_NONE;
	
	//mqttClient->pCon->proto.tcp = (esp_tcp *)os_zalloc(sizeof(esp_tcp));
	mqttClient->pCon->proto.tcp = &mqtt_tcp;
	mqttClient->pCon->proto.tcp->local_port = espconn_port();
	mqttClient->pCon->proto.tcp->remote_port = mqttClient->port;
	mqttClient->pCon->reverse = mqttClient;
	espconn_regist_connectcb(mqttClient->pCon, mqtt_tcpclient_connect_cb);
	espconn_regist_reconcb(mqttClient->pCon, mqtt_tcpclient_recon_cb);

	mqttClient->keepAliveTick = 0;
	mqttClient->reconnectTick = 0;


	os_timer_disarm(&mqttClient->mqttTimer);
	os_timer_setfn(&mqttClient->mqttTimer, (os_timer_func_t *)mqtt_timer, mqttClient);
	os_timer_arm(&mqttClient->mqttTimer, 1000, 1);
    os_printf("t1");
    os_printf("host: %s\r\n",mqttClient->host);
    os_printf("iP:"IPSTR"\r\n",IP2STR(mqttClient->pCon->proto.tcp->remote_ip));
	  
	if(UTILS_StrToIP(mqttClient->host, &mqttClient->pCon->proto.tcp->remote_ip)) {
		INFO("TCP: Connect to ip  %s:%d\r\n", mqttClient->host, mqttClient->port);
		if(mqttClient->security){
			os_printf("security flg\r\n");
			espconn_secure_connect(mqttClient->pCon);
		}
		else {
			espconn_connect(mqttClient->pCon);
		}
	}
	else {
		INFO("TCP: Connect to domain %s:%d\r\n", mqttClient->host, mqttClient->port);
		espconn_gethostbyname(mqttClient->pCon, mqttClient->host, &mqttClient->ip, mqtt_dns_found);
	}
	mqttClient->connState = TCP_CONNECTING;
}

void ICACHE_FLASH_ATTR
MQTT_FreeClient(MQTT_Client *mqttClient)
{
	
	if(mqttClient->pCon){
		INFO("Free memory\r\n");
		//if(mqttClient->pCon->proto.tcp)
		//	os_free(mqttClient->pCon->proto.tcp);
		//os_free(mqttClient->pCon);
		mqttClient->pCon->proto.tcp = NULL;
		mqttClient->pCon = NULL;
	}else{
		os_printf("mqttClient already null\r\n");
	}

	if(mqttClient->host){
		os_free(mqttClient->host);
		mqttClient->host = NULL;
	}
	
	if(mqttClient->connect_info.client_id){
		os_free(mqttClient->connect_info.client_id);
		mqttClient->connect_info.client_id = NULL;
	}
	
	if(mqttClient->connect_info.username){
		os_free(mqttClient->connect_info.username);
		mqttClient->connect_info.username = NULL;
	}
	
	if(mqttClient->connect_info.password){
		os_free(mqttClient->connect_info.password);
		mqttClient->connect_info.password = NULL;
	}
	if(mqttClient->connect_info.will_topic){
		os_free(mqttClient->connect_info.will_topic);
		mqttClient->connect_info.will_topic = NULL;
	}
	if(mqttClient->connect_info.will_message){
		os_free(mqttClient->connect_info.will_message);
		mqttClient->connect_info.will_message = NULL;
	}

	if(mqttClient->connect_info.will_topic){
		os_free(mqttClient->connect_info.will_topic);
		mqttClient->connect_info.will_topic=NULL;
	}
	if(mqttClient->connect_info.will_message){
		os_free(mqttClient->connect_info.will_message);
		mqttClient->connect_info.will_message=NULL;
	}

}
void ICACHE_FLASH_ATTR
MQTT_Disconnect(MQTT_Client *mqttClient)
{
    if(mqttClient){
		os_timer_disarm(&mqttClient->mqttTimer);
		INFO("MQTT:disarm mqttTimer\r\n");
    	if(mqttClient->pCon){
    		INFO("Free memory\r\n");
    		//if(mqttClient->pCon->proto.tcp)
    			//os_printf("flag 01\r\n");
    			//os_free(mqttClient->pCon->proto.tcp);
				//mqttClient->pCon->proto.tcp = NULL;
    			//os_printf("flag 02\r\n");
    		//os_free(mqttClient->pCon);
    		//os_printf("flag 03\r\n");
			espconn_delete(mqttClient->pCon);
    		mqttClient->pCon->proto.tcp = NULL;
    		mqttClient->pCon = NULL;
    	}else{
            os_printf("mqttClient already null\r\n");
		}

	}
	//os_printf("flag 04\r\n");
	//os_timer_disarm(&mqttClient->mqttTimer);
	//os_printf("flag 05\r\n");
}
void ICACHE_FLASH_ATTR
MQTT_OnConnected(MQTT_Client *mqttClient, MqttCallback connectedCb)
{
	mqttClient->connectedCb = connectedCb;
}

void ICACHE_FLASH_ATTR
MQTT_OnDisconnected(MQTT_Client *mqttClient, MqttCallback disconnectedCb)
{
	mqttClient->disconnectedCb = disconnectedCb;
}

void ICACHE_FLASH_ATTR
MQTT_OnData(MQTT_Client *mqttClient, MqttDataCallback dataCb)
{
	mqttClient->dataCb = dataCb;
}

void ICACHE_FLASH_ATTR
MQTT_OnPublished(MQTT_Client *mqttClient, MqttCallback publishedCb)
{
	mqttClient->publishedCb = publishedCb;
}
