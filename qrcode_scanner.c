
#include <assert.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include <zbar.h>

#include "common/sample_common.h"
#include "rkmedia_api.h"
#include "rkmedia_venc.h"

#if 0
#define DBG(x...) printf(x)
#else
#define DBG(x...) do {} while(0)
#endif

#define DEST_W 640
#define DEST_H 360

static RK_CHAR optstr[] = "?:w:h:a:";
static bool g_snap = false;

static RK_U32 g_width;
static RK_U32 g_height;

static char *result = NULL;

static bool quit = false;
static void sigterm_handler(int sig)
{
  fprintf(stderr, "signal %d\n", sig);
  quit = true;
}

static int zbar_qrcode_scan(void *yuv_data, int sz, int w, int h)
{
  int ret = -1;
  DBG(">>> %s\n", __func__);
  zbar_image_scanner_t *scanner = NULL;
  if(yuv_data == NULL) 
  {
      return -1;
  }
  scanner = zbar_image_scanner_create();
  zbar_image_scanner_set_config(scanner, 0, ZBAR_CFG_ENABLE, 1);  

  zbar_image_t *image = zbar_image_create();
  zbar_image_set_format(image, zbar_fourcc('N', 'V', '1', '2'));
  zbar_image_set_size(image, w, h);
  zbar_image_set_data(image, yuv_data, w * h, zbar_image_free_data);

  zbar_image_t *in_img = zbar_image_convert(image, zbar_fourcc('Y', '8', '0', '0'));
  int n = zbar_scan_image(scanner, in_img);
  const zbar_symbol_t *symbol = zbar_image_first_symbol(in_img);
  if (NULL == symbol)
  {
      DBG("~~~ %s: parse qrcode failed!\n", __func__);
      goto exit;
  }
  for(; symbol; symbol = zbar_symbol_next(symbol)) 
  {
      zbar_symbol_type_t typ = zbar_symbol_get_type(symbol);
      const char *data = zbar_symbol_get_data(symbol);
      if(data != NULL) 
      {
          unsigned int data_len = zbar_symbol_get_data_length(symbol);
          result = (char *)malloc((size_t)data_len + 1);
          if(!result) {
            continue;
          }
          strncpy(result, data, data_len);
          *(result + data_len) = '\0';
          ret = 0;
          printf("=== decoded [%s] symbol\n", zbar_get_symbol_name(typ));
          printf("%s\n", data);
      }
  }
exit:
  zbar_image_destroy(image);
  zbar_image_destroy(in_img);
  zbar_image_scanner_destroy(scanner);
  return ret;
}

static void *process(void *arg)
{
  int ret;
  MEDIA_BUFFER mb = NULL;

  while (!quit)
  {
    mb = RK_MPI_SYS_GetMediaBuffer(RK_ID_RGA, 0, -1);
    if (!mb) {
      printf("RK_MPI_SYS_GetMediaBuffer get null buffer!\n");
      break;
    }

    if(g_snap) {
      void *data = RK_MPI_MB_GetPtr(mb);
      size_t size = RK_MPI_MB_GetSize(mb);
      void *buff = malloc(size);
      memcpy(buff, data, size);

      RK_MPI_MB_ReleaseBuffer(mb);
      g_snap  = false;

      ret = zbar_qrcode_scan(buff, size, g_width, g_height);
      if(ret) {
        continue;
      } else {
        quit = true;
        break;
      }
    }

#if 0
    DBG("Get Frame:ptr:%p, fd:%d, size:%zu, mode:%d, channel:%d, "
           "timestamp:%lld\n",
           RK_MPI_MB_GetPtr(mb), RK_MPI_MB_GetFD(mb), RK_MPI_MB_GetSize(mb),
           RK_MPI_MB_GetModeID(mb), RK_MPI_MB_GetChannelID(mb),
           RK_MPI_MB_GetTimestamp(mb));
#endif
    RK_MPI_MB_ReleaseBuffer(mb);
  }

  return NULL;
}

static void *setup_snap(void *arg)
{
  while (!quit) {
    g_snap = true;
    usleep(1000 * 1000);
  }
  return NULL;
}

int main(int argc, char *argv[])
{
  int ret = 0;
  int c;

  g_width = DEST_W;
  g_height = DEST_H;
  char *iq_file_dir = NULL;

  while ((c = getopt(argc, argv, optstr)) != -1) {
    switch(c) {
      case 'w':
        g_width = atoi(optarg);
        break;
      case 'h':
        g_height = atoi(optarg);
        break;
      case 'a':
        iq_file_dir = optarg;
        break;
      default:
        break;
    }
  }

  /* MPI Init */
  RK_MPI_SYS_Init();
#ifdef RKAIQ
  rk_aiq_working_mode_t hdr_mode = RK_AIQ_WORKING_MODE_NORMAL;
  RK_BOOL fec_enable = RK_FALSE;
  int fps = 30;

  SAMPLE_COMM_ISP_Init(hdr_mode, fec_enable, iq_file_dir);
  SAMPLE_COMM_ISP_Run();
  SAMPLE_COMM_ISP_SetFrameRate(fps);
#else
  (void)argc;
  (void)argv;
#endif
  
  /* Create VI */
  VI_CHN_ATTR_S vi_chn_attr;
  vi_chn_attr.pcVideoNode = "rkispp_scale0";
  vi_chn_attr.u32BufCnt = 4;
  vi_chn_attr.u32Width = 1920;
  vi_chn_attr.u32Height = 1080;
  vi_chn_attr.enPixFmt = IMAGE_TYPE_NV12;
  vi_chn_attr.enWorkMode = VI_WORK_MODE_NORMAL;
  ret = RK_MPI_VI_SetChnAttr(0, 1, &vi_chn_attr);
  ret |= RK_MPI_VI_EnableChn(0, 1);
  if (ret) {
    printf("Create vi[1] failed! ret=%d\n", ret);
    return -1;
  }

  /* Create RGA */
  RGA_ATTR_S stRgaAttr;
  stRgaAttr.bEnBufPool = RK_TRUE;
  stRgaAttr.u16BufPoolCnt = 12;
  stRgaAttr.u16Rotaion = 0;
  stRgaAttr.stImgIn.u32X = 0;
  stRgaAttr.stImgIn.u32Y = 0;
  stRgaAttr.stImgIn.imgType = IMAGE_TYPE_NV12;
  stRgaAttr.stImgIn.u32Width = 1920;
  stRgaAttr.stImgIn.u32Height = 1080;
  stRgaAttr.stImgIn.u32HorStride = 1920;
  stRgaAttr.stImgIn.u32VirStride = 1080;

  stRgaAttr.stImgOut.u32X = 0;
  stRgaAttr.stImgOut.u32Y = 0;
  stRgaAttr.stImgOut.imgType = IMAGE_TYPE_NV12;
  stRgaAttr.stImgOut.u32Width = g_width;
  stRgaAttr.stImgOut.u32Height = g_height;
  stRgaAttr.stImgOut.u32HorStride = g_width;
  stRgaAttr.stImgOut.u32VirStride = g_height;

  ret = RK_MPI_RGA_CreateChn(0, &stRgaAttr);
  if (ret) {
    printf("Create rga[0] falied! ret=%d\n", ret);
    return -1;
  }

  /* create thread to get buffer */
  pthread_t read_thread;
  pthread_create(&read_thread, NULL, process, NULL);

  /* bind VI and RGA */
  MPP_CHN_S stSrcChn;
  stSrcChn.enModId = RK_ID_VI;
  stSrcChn.s32DevId = 0;
  stSrcChn.s32ChnId = 1;
  MPP_CHN_S stDestChn;
  stDestChn.enModId = RK_ID_RGA;
  stDestChn.s32DevId = 0;
  stDestChn.s32ChnId = 0;
  ret = RK_MPI_SYS_Bind(&stSrcChn, &stDestChn);
  if (ret) {
    printf("Bind vi[1] and rga[0] failed! ret=%d\n", ret);
    return -1;
  }

  printf("%s initial finish\n", __func__);
  signal(SIGINT, sigterm_handler);

  pthread_t setup_thread;
  pthread_create(&setup_thread, NULL, setup_snap, NULL);

  while(!quit) {
    usleep(100 * 1000);
  }

  pthread_join(setup_thread, NULL);  
  pthread_join(read_thread, NULL); 

#ifdef RKAIQ
  SAMPLE_COMM_ISP_Stop();
#endif

  RK_MPI_SYS_UnBind(&stSrcChn, &stDestChn);
  RK_MPI_RGA_DestroyChn(0);
  RK_MPI_VI_DisableChn(0, 1);

  if(result) {
    printf("\n\nresult:\n");
    printf("%s \n\n", result);
  }

  printf("exit\n");

  return 0;
}
