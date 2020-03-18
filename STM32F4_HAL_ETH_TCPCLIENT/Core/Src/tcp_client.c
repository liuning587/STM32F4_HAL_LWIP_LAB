/*
 * ntp_client.c
 *
 *  Created on: 2020. 3. 12.
 *      Author: KIKI
 */

#include <tcp_client.h>

static struct tcp_pcb *pcb_client; //client pcb
static ip_addr_t server_addr; //server ip

struct time_packet packet;
uint16_t written = 0;

/* callback functions */
static err_t tcp_callback_connected(void *arg, struct tcp_pcb *pcb_new, err_t err);
static err_t tcp_callback_sent(void *arg, struct tcp_pcb *tpcb, u16_t len);
static err_t tcp_callback_received(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);
static err_t tcp_callback_poll(void *arg, struct tcp_pcb *tpcb);
static void tcp_callback_error(void *arg, err_t err);

/* functions */
static err_t app_open_conn(void); //open function
static void app_close_conn(void); //close function
static void app_send_data(void); //send function

/* start get time */
void app_start_get_time(void)
{
  //tcp_connect => tcp_callback_connected => tcp_write => tcp_callback_received => tcp_close
  app_open_conn();
}

/*
 * app_open_connection 에서는 pcb 생성 후 tcp_connect 를 호출하여 서버에 연결한다.
 */
static err_t app_open_conn(void)
{
  err_t err;

  if (pcb_client == NULL)
  {
    pcb_client = tcp_new(); //allocate pcb memory
    if (pcb_client == NULL)
    {
      memp_free(MEMP_TCP_PCB, pcb_client); //lack of memory
      pcb_client = NULL;
      HAL_GPIO_WritePin(LED_BLUE_GPIO_Port, LED_BLUE_Pin, GPIO_PIN_SET); //error led
      return ERR_MEM;
    }
  }

  IP4_ADDR(&server_addr, SERVER_IP1, SERVER_IP2, SERVER_IP3, SERVER_IP4);
  err = tcp_connect(pcb_client, &server_addr, SERVER_PORT, tcp_callback_connected); //connect

  if(err == ERR_ISCONN) //not closed yet
  {
    app_close_conn();
  }

  return err;
}

/*
 * tcp_callback_connected 에서는 연결 성공 시 리퀘스트를 전송한다.
 */
static err_t tcp_callback_connected(void *arg, struct tcp_pcb *pcb_new, err_t err)
{
  LWIP_UNUSED_ARG(arg);

  if (err != ERR_OK) //error when connected
  {
    return err;
  }

  tcp_setprio(pcb_new, TCP_PRIO_NORMAL); //set priority for new pcb

  tcp_arg(pcb_new, 0); //no arg
  tcp_sent(pcb_new, tcp_callback_sent); //register send callback
  tcp_recv(pcb_new, tcp_callback_received);  //register receive callback
  tcp_err(pcb_new, tcp_callback_error); //register error callback
  tcp_poll(pcb_new, tcp_callback_poll, 0); //register poll callback

  app_send_data(); //send request

  return ERR_OK;
}

/*
 * app_send_data 에서는 time 리퀘스트를 전송한다.
 */
static void app_send_data(void)
{
  memset(&packet, 0, sizeof(struct time_packet));
  packet.head = 0xAE;
  packet.type = REQ;
  packet.tail = 0xEA;

  tcp_write(pcb_client, &packet,sizeof(struct time_packet), TCP_WRITE_FLAG_COPY);
  tcp_output(pcb_client); //flush
}

/*
 * tcp_callback_sent 은 전송 완료 시 호출된다.
 */
static err_t tcp_callback_sent(void *arg, struct tcp_pcb *tpcb, u16_t len)
{
  LWIP_UNUSED_ARG(arg);
  LWIP_UNUSED_ARG(tpcb);
  LWIP_UNUSED_ARG(len);

  if(len != sizeof(struct time_packet))
  {
    HAL_GPIO_WritePin(LED_BLUE_GPIO_Port, LED_BLUE_Pin, GPIO_PIN_SET); //error led
  }
  else
  {
    HAL_GPIO_TogglePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin); //blink green when sent O.K
  }

  return ERR_OK;
}

/*
 * tcp_callback_received 은 데이터 수신 시 호출된다.
 */
static err_t tcp_callback_received(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
  err_t ret_err;

  if (p == NULL) //연결이 close 되었을 때 호출된다.
  {
    app_close_conn();
    ret_err = ERR_OK;
  }
  else if (err != ERR_OK) //tcp_abort 를 호출하였을 때 ERR_ABRT 가 들어온다.
  {
    tcp_recved(tpcb, p->tot_len); //advertise window size

    pbuf_free(p); //clear buffer
    ret_err = err;
  }
  else //receiving data
  {
    tcp_recved(tpcb, p->tot_len);

    memcpy(&packet + written, p->payload, p->len);
    written += p->len;

    if(written == sizeof(struct time_packet) && packet.type == RESP)
    {
      written = 0;

      printf("%04d-%02d-%02d %02d:%02d:%02d\n",
             packet.year + 2000,
             packet.month, packet.day, packet.hour, packet.minute, packet.second);

      app_close_conn();
    }

    pbuf_free(p);
    ret_err = ERR_OK;
  }

  return ret_err;
}

/* close connection */
static void app_close_conn(void)
{
  /* clear callback functions */
  tcp_arg(pcb_client, NULL);
  tcp_sent(pcb_client, NULL);
  tcp_recv(pcb_client, NULL);
  tcp_err(pcb_client, NULL);
  tcp_poll(pcb_client, NULL, 0);

  tcp_close(pcb_client);    //close connection
  pcb_client = NULL;
}

/* error callback */
static void tcp_callback_error(void *arg, err_t err)
{
  LWIP_UNUSED_ARG(arg);
  LWIP_UNUSED_ARG(err);

  HAL_GPIO_WritePin(LED_BLUE_GPIO_Port, LED_BLUE_Pin, GPIO_PIN_SET); //error loed
}

/* poll callback */
static err_t tcp_callback_poll(void *arg, struct tcp_pcb *tpcb)
{
  return ERR_OK;
}
