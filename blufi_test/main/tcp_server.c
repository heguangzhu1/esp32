/* BSD Socket API Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>
#include "esp_netif.h"
#include <stdio.h>
#include <stdlib.h>
#include "driver/uart.h"
#include "freertos/queue.h"
#include "sdkconfig.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_netif_types.h"

//#define PORT                        CONFIG_EXAMPLE_PORT
//#define KEEPALIVE_IDLE              CONFIG_EXAMPLE_KEEPALIVE_IDLE
//#define KEEPALIVE_INTERVAL          CONFIG_EXAMPLE_KEEPALIVE_INTERVAL
//#define KEEPALIVE_COUNT             CONFIG_EXAMPLE_KEEPALIVE_COUNT

#define PORT                        3333
#define KEEPALIVE_IDLE              5
#define KEEPALIVE_INTERVAL          5
#define KEEPALIVE_COUNT             5
static const char *TAG = "example";
//信号量
SemaphoreHandle_t TaskToTrqSemaphoreCounting;
#define CONFIG_EXAMPLE_IPV4
#define TCP_SERVER_PORT 8080   //服务器监听的端口号

#define TCP_SERVER_MAX_COUNT 6    //设置最大支持的TCP客户端连接个数(超过6需要用户自己增加回调函数)
#define TCP_SERVER_MIN_PRIORITY 8 //客户端接收数据任务最低优先级(6个客户端的优先级分别为: 8,9,10,11,12,13)

typedef void(*tcp_client_recv_callback)(void *pvParameters);//定义一个函数指针类型
void esp_netif_action_got_ip(void *esp_netif, esp_event_base_t base, int32_t event_id, void *data);

static void tcp_flag_task0(int sock);
static void tcp_flag_task1(int sock);
int sendstop_flag0=1;
 int sendstop_flag1=1;

void ota_task_he();

typedef struct tcp_server_typedef_struct
{
    char state;//状态
    int  sock_id;
    tcp_client_recv_callback tcp_client_recv_cb;//存储tcp接收函数地址
} tcp_server_struct;

 tcp_server_struct tcp_server_struct_value[TCP_SERVER_MAX_COUNT];


/* 485   ************************************************************************/
#define ECHO_TEST_CTS   (UART_PIN_NO_CHANGE)

#define RX_BUF_SIZE     (1024)
#define BAUD_RATE       (CONFIG_ECHO_UART_BAUD_RATE)


#define PACKET_READ_TICS        (10 / portTICK_RATE_MS)

uint8_t  TX_BUF[3] ={0xfd,0x00,0x00};
uint8_t  err_cnt[18]={0};
uint8_t rel_data[18]={0};
uint8_t wifi_data[182]={0};
uint8_t wifi_data2[182]={0};
int read_done=0;
int tx_flag=0;
int num=0;
uint sum_err_cnt = 0;
uint sum_right_cnt = 0;

//extern void esp_netif_action_got_ip(void *esp_netif, esp_event_base_t base, int32_t event_id, void *data);
//char ip_adress;
//
//void esp_netif_action_got_ip(void *esp_netif, esp_event_base_t base, int32_t event_id, void *data)
//{
//    ESP_LOGD(TAG, "esp_netif action got_ip with netif%p from event_id=%d", esp_netif, event_id);
//    const ip_event_got_ip_t *event = (const ip_event_got_ip_t *) data;
//    ESP_LOGI(TAG, "%s ip: " IPSTR ", mask: " IPSTR ", gw: " IPSTR, esp_netif_get_desc(esp_netif),
//             IP2STR(&event->ip_info.ip),
//             IP2STR(&event->ip_info.netmask),
//             IP2STR(&event->ip_info.gw));
//    ip_adress=(char)IP2STR(&event->ip_info.ip);
//
//}

     void uart_init(void)
     {
     			uart_config_t	uart_config =
     				{
     					.baud_rate			= 115200,
     					.data_bits			= UART_DATA_8_BITS,
     					.parity 			= UART_PARITY_DISABLE,
     					.stop_bits	        = UART_STOP_BITS_1,
     					.flow_ctrl			= UART_HW_FLOWCTRL_DISABLE
     				};
     	 // Install UART driver (we don't need an event queue here)
          // In this example we don't even use a buffer for sending data.
     	 ESP_ERROR_CHECK(uart_driver_install(2, RX_BUF_SIZE*2 , 0, 0, NULL, 0));
     	 // Configure UART parameters
     	 ESP_ERROR_CHECK(uart_param_config(2, &uart_config));
         // Set UART pins as per KConfig settings
         ESP_ERROR_CHECK(uart_set_pin(2, 17, 16, -1, -1));
         uart_driver_install(1,RX_BUF_SIZE*2,0,0,NULL,0);
     	uart_param_config(1, &uart_config);
     	uart_set_pin(1,10,9,-1,-1);
     }



     int  data_sum(uint8_t *a, int b)
     {
     	int sum =0;
     	int len = b;
     	uint8_t i=0;
     	for (int i=0;i<len-1;i++)
     	{
     		sum=sum+*(a+i);
     	}
     	i=sum;
     	return i;
     }

     void data_transmit(void)
     {
     	uart_flush(2);
     	TX_BUF[1]=TX_BUF[1]+1;
     	if(TX_BUF[1]==19)
     	{
     		TX_BUF[1]=0x01;
     	}
     	TX_BUF[2]=data_sum(TX_BUF,3);
     	gpio_set_level(4, 1);
     	for(int i=0;i<3;i++){
     	uart_write_bytes(2,&TX_BUF[i],1);
     	uart_wait_tx_done(2,1);
     	}
     	gpio_set_level(4, 0);
     	err_cnt[TX_BUF[1]]=err_cnt[TX_BUF[1]]+1;
         uint8_t data[18];
         int rxBytes = uart_read_bytes(2, data, 18,10/ portTICK_RATE_MS);
             if(rxBytes>0)
             {
             	for(int i=0;i<rxBytes;i++)
             	        	{
             	        		rel_data[i] = data[i];
             	        		printf("%.2X",(uint8_t)data[i]);
             	        	}
             	read_done = 1;
             	printf("\r\n");
            }
     }

     void data_processing(void )
     {
     	int j=0;
     	int len=sizeof(rel_data);
     	if(read_done)
         {
     	    if(rel_data[0]==254  && data_sum(rel_data,18)==rel_data[17] && len==18)
             {
     	    	err_cnt[TX_BUF[1]]=0;
     	    	wifi_data[0]=254;
     	    	sum_right_cnt=sum_right_cnt+1;
     	        j=2;
     			for(int i=1;i<6;i++)
     			{
     				num++;
     				wifi_data[num]=(TX_BUF[1]*5-5)+i;
     				num++;
     				wifi_data[num]=(rel_data[j+1]*256+rel_data[j+2])/40;
     				j=j+3;
     			}
            }
     	   else//У��ʹ���
     	   {
     		   sum_err_cnt=sum_err_cnt+1;
     		   j=2;
     			for(int i=1;i<6;i++)
     			{
     				num++;
     				wifi_data[num]=(TX_BUF[1]*5-5)+i;
     				num++;
     				j=j+3;
     			}
           }
         }
         else//û�н��յ�����
         {
         	if(err_cnt[TX_BUF[1]]>2)//3������
         	{
         		j=2;
     			for(int i=1;i<6;i++)
     			{
     				num++;
     				wifi_data[num]=(TX_BUF[1]*5-5)+i;
     				num++;
     				wifi_data[num]=0xff;
     				j=j+3;
     			}
         	}
         	else//3������
         	{
               j=2;
     			for(int i=1;i<6;i++)
     			{
     				num++;
     				wifi_data[num]=(TX_BUF[1]*5-5)+i;
     				num++;
     				//wifi_data[num]=wifi_data[num];
     				j=j+3;
     			}
         	}

         }
     	//printf("%d\r\n",num);
     	if(num==180)
     	  {
     		   wifi_data[181]=data_sum(wifi_data,182);
     		   tx_flag=1;
     		   num=0;
     	  }

         read_done = 0;
         for(int i=0;i<18;i++)
            	{
          		rel_data[i]=0x00;
            	}
     }
/* 485   ************************************************************************/



     static void tcp_flag_task0(int sock)
     {
     	int len=sizeof(wifi_data);
     	while(sendstop_flag0){
//     		if(tx_flag){
//     			for(int i=0;i<182;i++)
//     			wifi_data2[i]=wifi_data[i];
//     			tx_flag=0;
//     		}
     	send(sock, wifi_data, len, 0);
     	vTaskDelay(50/ portTICK_RATE_MS);
     	}
     	vTaskDelete(NULL);
     	}

          void tcp_client0_recv(void *pvParameters)
          {
        	  sendstop_flag0=1;
        	  int len;
        	  char rx_buffer[128];
        	  int sock=tcp_server_struct_value[0].sock_id;
        	  xTaskCreate(tcp_flag_task0, "tcp_flag_task0", 4096, sock, 10, NULL);

              do {
                  len = recv(sock, rx_buffer, sizeof(rx_buffer)-1, 0);
                  if (len < 0) {
                      ESP_LOGE(TAG, "Error occurred during receiving: errno %d", errno);sendstop_flag0=0;

                  } else if (len == 0) {
                      ESP_LOGW(TAG, "Connection closed");sendstop_flag0=0;
                      vTaskDelay(10/ portTICK_RATE_MS);
                  } else {
                      rx_buffer[len] = 0; // Null-terminate whatever is received and treat it like a string
                      ESP_LOGI(TAG, "Received %d bytes: %s", len, rx_buffer);
                      if(!(strcmp(rx_buffer,"otaupdate")))
                      {
                        printf("ota start!\n");


                        //ota_task_he();

                      }
                  }
              } while (len > 0);

              tcp_server_struct_value[0].state=0;
              xSemaphoreGive(TaskToTrqSemaphoreCounting);
              shutdown(sock, 0);
              close(sock);
              vTaskDelete(NULL);
          }

     static void tcp_flag_task1(int sock)
     {
    	 int len=sizeof(wifi_data);
    	 	while(sendstop_flag1)
    	 	{
    	     	send(sock, wifi_data, len, 0);
    	    	vTaskDelay(50/ portTICK_RATE_MS);
    	    }
    	      	vTaskDelete(NULL);
     }

     void tcp_client1_recv(void *pvParameters)
     {
    	 sendstop_flag1=1;
         int len;
         char rx_buffer[128];
         int sock=tcp_server_struct_value[1].sock_id;
         xTaskCreate(tcp_flag_task1, "tcp_flag_task1", 4096, sock, 11, NULL);
         do {
             len = recv(sock, rx_buffer, sizeof(rx_buffer)-1, 0);
             if (len < 0) {
                 ESP_LOGE(TAG, "Error occurred during receiving: errno %d", errno);sendstop_flag1=0;
                 vTaskDelay(10/ portTICK_RATE_MS);
             } else if (len == 0) {
                 ESP_LOGW(TAG, "Connection closed");sendstop_flag1=0;
                 vTaskDelay(10/ portTICK_RATE_MS);
             } else {
                 rx_buffer[len] = 0; // Null-terminate whatever is received and treat it like a string
                 ESP_LOGI(TAG, "Received %d bytes: %s", len, rx_buffer);

                 // send() can return less bytes than supplied length.
                 // Walk-around for robust implementation.
                 int to_write = len;
                 while (to_write > 0) {
                     int written = send(sock, rx_buffer + (len - to_write), to_write, 0);
                     if (written < 0) {
                         ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
                     }
                     to_write -= written;
                 }
             }
         } while (len > 0);
                tcp_server_struct_value[1].state=0;
         xSemaphoreGive(TaskToTrqSemaphoreCounting);
         shutdown(sock, 0);
         close(sock);
         vTaskDelete(NULL);
     }

     void tcp_client2_recv(void *pvParameters)
     {
         int len;
         char rx_buffer[128];
         int sock=tcp_server_struct_value[2].sock_id;
         do {
             len = recv(sock, rx_buffer, sizeof(rx_buffer)-1, 0);
             if (len < 0) {
                 ESP_LOGE(TAG, "Error occurred during receiving: errno %d", errno);
             } else if (len == 0) {
                 ESP_LOGW(TAG, "Connection closed");
             } else {
                 rx_buffer[len] = 0; // Null-terminate whatever is received and treat it like a string
                 ESP_LOGI(TAG, "Received %d bytes: %s", len, rx_buffer);

                 // send() can return less bytes than supplied length.
                 // Walk-around for robust implementation.
                 int to_write = len;
                 while (to_write > 0) {
                     int written = send(sock, rx_buffer + (len - to_write), to_write, 0);
                     if (written < 0) {
                         ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
                     }
                     to_write -= written;
                 }
             }
         } while (len > 0);

         tcp_server_struct_value[2].state=0;
         xSemaphoreGive(TaskToTrqSemaphoreCounting);
         shutdown(sock, 0);
         close(sock);
         vTaskDelete(NULL);
     }

     void tcp_client3_recv(void *pvParameters)
     {
         int len;
         char rx_buffer[128];
         int sock=tcp_server_struct_value[3].sock_id;
         do {
             len = recv(sock, rx_buffer, sizeof(rx_buffer)-1, 0);
             if (len < 0) {
                 ESP_LOGE(TAG, "Error occurred during receiving: errno %d", errno);
             } else if (len == 0) {
                 ESP_LOGW(TAG, "Connection closed");
             } else {
                 rx_buffer[len] = 0; // Null-terminate whatever is received and treat it like a string
                 ESP_LOGI(TAG, "Received %d bytes: %s", len, rx_buffer);

                 // send() can return less bytes than supplied length.
                 // Walk-around for robust implementation.
                 int to_write = len;
                 while (to_write > 0) {
                     int written = send(sock, rx_buffer + (len - to_write), to_write, 0);
                     if (written < 0) {
                         ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
                     }
                     to_write -= written;
                 }
             }
         } while (len > 0);

         tcp_server_struct_value[3].state=0;
         xSemaphoreGive(TaskToTrqSemaphoreCounting);
         shutdown(sock, 0);
         close(sock);
         vTaskDelete(NULL);
     }

     void tcp_client4_recv(void *pvParameters)
     {
         int len;
         char rx_buffer[128];
         int sock=tcp_server_struct_value[4].sock_id;
         do {
             len = recv(sock, rx_buffer, sizeof(rx_buffer)-1, 0);
             if (len < 0) {
                 ESP_LOGE(TAG, "Error occurred during receiving: errno %d", errno);
             } else if (len == 0) {
                 ESP_LOGW(TAG, "Connection closed");
             } else {
                 rx_buffer[len] = 0; // Null-terminate whatever is received and treat it like a string
                 ESP_LOGI(TAG, "Received %d bytes: %s", len, rx_buffer);

                 // send() can return less bytes than supplied length.
                 // Walk-around for robust implementation.
                 int to_write = len;
                 while (to_write > 0) {
                     int written = send(sock, rx_buffer + (len - to_write), to_write, 0);
                     if (written < 0) {
                         ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
                     }
                     to_write -= written;
                 }
             }
         } while (len > 0);

         tcp_server_struct_value[4].state=0;
         xSemaphoreGive(TaskToTrqSemaphoreCounting);
         shutdown(sock, 0);
         close(sock);
         vTaskDelete(NULL);
     }

     void tcp_client5_recv(void *pvParameters)
     {
         int len;
         char rx_buffer[128];
         int sock=5;
         do {
             len = recv(sock, rx_buffer, sizeof(rx_buffer)-1, 0);
             if (len < 0) {
                 ESP_LOGE(TAG, "Error occurred during receiving: errno %d", errno);
             } else if (len == 0) {
                 ESP_LOGW(TAG, "Connection closed");
             } else {
                 rx_buffer[len] = 0; // Null-terminate whatever is received and treat it like a string
                 ESP_LOGI(TAG, "Received %d bytes: %s", len, rx_buffer);

                 // send() can return less bytes than supplied length.
                 // Walk-around for robust implementation.
                 int to_write = len;
                 while (to_write > 0) {
                     int written = send(sock, rx_buffer + (len - to_write), to_write, 0);
                     if (written < 0) {
                         ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
                     }
                     to_write -= written;
                 }
             }
         } while (len > 0);

         tcp_server_struct_value[5].state=0;
         xSemaphoreGive(TaskToTrqSemaphoreCounting);
         shutdown(sock, 0);
         close(sock);
         vTaskDelete(NULL);
     }







static void tcp_server_task(void *pvParameters)
{
	int i;
    char addr_str[128];
    int addr_family = (int)pvParameters;
    int ip_protocol = 0;
    int keepAlive = 1;
    int keepIdle = KEEPALIVE_IDLE;
    int keepInterval = KEEPALIVE_INTERVAL;
    int keepCount = KEEPALIVE_COUNT;
    struct sockaddr_storage dest_addr;

    if (addr_family == AF_INET) {   //AF_INET==2
        struct sockaddr_in *dest_addr_ip4 = (struct sockaddr_in *)&dest_addr;
        dest_addr_ip4->sin_addr.s_addr = htonl(INADDR_ANY);
        dest_addr_ip4->sin_family = AF_INET;
        dest_addr_ip4->sin_port = htons(PORT);
        ip_protocol = IPPROTO_IP;
    }
#ifdef CONFIG_EXAMPLE_IPV6
    else if (addr_family == AF_INET6) {  //AF_INET6==2
        struct sockaddr_in6 *dest_addr_ip6 = (struct sockaddr_in6 *)&dest_addr;
        bzero(&dest_addr_ip6->sin6_addr.un, sizeof(dest_addr_ip6->sin6_addr.un));
        dest_addr_ip6->sin6_family = AF_INET6;
        dest_addr_ip6->sin6_port = htons(PORT);
        ip_protocol = IPPROTO_IPV6;
    }
#endif

    int listen_sock = socket(addr_family, SOCK_STREAM, ip_protocol);
    if (listen_sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        vTaskDelete(NULL);
        return;
    }
    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#if defined(CONFIG_EXAMPLE_IPV4) && defined(CONFIG_EXAMPLE_IPV6)
    // Note that by default IPV6 binds to both protocols, it is must be disabled
    // if both protocols used at the same time (used in CI)
    setsockopt(listen_sock, IPPROTO_IPV6, IPV6_V6ONLY, &opt, sizeof(opt));
#endif

    ESP_LOGI(TAG, "Socket created");

    int err = bind(listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err != 0) {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        ESP_LOGE(TAG, "IPPROTO: %d", addr_family);
        goto CLEAN_UP;
    }
    ESP_LOGI(TAG, "Socket bound, port %d", PORT);

    err = listen(listen_sock, 1);
    if (err != 0) {
        ESP_LOGE(TAG, "Error occurred during listen: errno %d", errno);
        goto CLEAN_UP;
    }

    //创建信号量
        TaskToTrqSemaphoreCounting=xSemaphoreCreateCounting( TCP_SERVER_MAX_COUNT, TCP_SERVER_MAX_COUNT );//最大计数值、刚创建时的计数值
        struct sockaddr_in6 source_addr; // Large enough for both IPv4 or IPv6
        uint addr_len = sizeof(source_addr);

        for(i=0;i<TCP_SERVER_MAX_COUNT;i++) tcp_server_struct_value[i].state=0;

        /*如果修改了客户端个数,需要修改这个地方,设置回调函数*/
        tcp_server_struct_value[0].tcp_client_recv_cb = tcp_client0_recv;
        tcp_server_struct_value[1].tcp_client_recv_cb = tcp_client1_recv;
        tcp_server_struct_value[2].tcp_client_recv_cb = tcp_client2_recv;
        tcp_server_struct_value[3].tcp_client_recv_cb = tcp_client3_recv;
        tcp_server_struct_value[4].tcp_client_recv_cb = tcp_client4_recv;
        tcp_server_struct_value[5].tcp_client_recv_cb = tcp_client5_recv;



    while (1) {
    	 if(xSemaphoreTake(TaskToTrqSemaphoreCounting,portMAX_DELAY))
        {ESP_LOGI(TAG, "Socket listening");

        struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
        socklen_t addr_len = sizeof(source_addr);
        int sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
        if (sock < 0) {
            ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
            break;
        }

        // Set tcp keepalive option
        setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &keepAlive, sizeof(int));
        setsockopt(sock, IPPROTO_TCP, TCP_KEEPIDLE, &keepIdle, sizeof(int));
        setsockopt(sock, IPPROTO_TCP, TCP_KEEPINTVL, &keepInterval, sizeof(int));
        setsockopt(sock, IPPROTO_TCP, TCP_KEEPCNT, &keepCount, sizeof(int));
        // Convert ip address to string
        if (source_addr.ss_family == PF_INET) {
            inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr, addr_str, sizeof(addr_str) - 1);
        }
#ifdef CONFIG_EXAMPLE_IPV6
        else if (source_addr.ss_family == PF_INET6) {
            inet6_ntoa_r(((struct sockaddr_in6 *)&source_addr)->sin6_addr, addr_str, sizeof(addr_str) - 1);
        }
#endif
        ESP_LOGI(TAG, "Socket accepted ip address: %s", addr_str);

        for(i=0;i<TCP_SERVER_MAX_COUNT;i++)
              {
                  if(tcp_server_struct_value[i].state==0)
                  {
                      tcp_server_struct_value[i].state=1;
                      tcp_server_struct_value[i].sock_id = sock;
                      xTaskCreate(tcp_server_struct_value[i].tcp_client_recv_cb, "tcp_server_client", 4096, (void*)&tcp_server_struct_value[i], TCP_SERVER_MIN_PRIORITY+i, NULL);
                      break;
                  }
              }
    }

    }

CLEAN_UP:
    close(listen_sock);
    vTaskDelete(NULL);
}

static void tcp_data_task()
{
	   while(1)
	      	{
	      		vTaskDelay(10/ portTICK_RATE_MS);
	      		data_transmit();
	      		data_processing();
	      	}
	   vTaskDelete(NULL);
}

void tcp_task_he(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());
	uart_init();
	gpio_pad_select_gpio(4);//IO4
	gpio_set_direction(4, GPIO_MODE_OUTPUT);//输出
	gpio_set_level(4, 1); //拉低
	vTaskDelay(1000/ portTICK_RATE_MS);
	uart_write_bytes(1,"\r\n",2);
    xTaskCreate(tcp_data_task, "tcp_data_task", 4096, NULL, 9, NULL);

#ifdef CONFIG_EXAMPLE_IPV4
    xTaskCreate(tcp_server_task, "tcp_server", 4096, (void*)AF_INET, 5, NULL);
#endif
#ifdef CONFIG_EXAMPLE_IPV6
    xTaskCreate(tcp_server_task, "tcp_server", 4096, (void*)AF_INET6, 5, NULL);
#endif


}

void app_main(void)
{

    tcp_task_he();
    printf("yes ");
    printf("第一次使用GitHub")；

}

