/* 
 * Read temperature & humidity data from Adafruit SHT41 USB device.
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <gtk/gtk.h>

#define DEVICE "/dev/serial/by-id/usb-Adafruit_SHT4x_Trinkey_M0_C2CD0A4F503059384B2E3120FF0E230D-if00"

// Update every 5 sec (5000 ms)
#define UPDATE 5000

int fd;
double cur_temp = -1.0, cur_hum = -1.0;
GtkWidget *temp, *hum;

int read_untilcr(char *buf, int maxlen) {
  
  int pos = 0, bytes_rd;
  
  while(1) {
    if((bytes_rd = read(fd, buf + pos, 1)) < 0) return -1;
    if(bytes_rd == 0) {
      sleep(1); // could be less than 1sec
      continue;
    }
    if(!pos && (*buf == '\r' || *buf == '\n')) continue;
    if(pos == maxlen) return -1;
    if(buf[pos] == '\r') break;
    pos++;
  }
  buf[pos] = '\0';
  return pos;
}

void set_tty() {
  
  struct termios param;
  
  if(tcgetattr(fd, &param) < 0) {
    fprintf(stderr, "Status get failed.\n");
    exit(1);
  }
  
  param.c_cflag &= ~PARENB;
  param.c_cflag &= ~CSTOPB;
  param.c_cflag |= CS8;
  param.c_cflag &= ~CRTSCTS;
  param.c_cflag |= CREAD | CLOCAL;
  param.c_lflag &= ~ICANON;
  param.c_lflag &= ~ECHO;
  param.c_lflag &= ~ECHOE;
  param.c_lflag &= ~ECHONL;
  param.c_lflag &= ~ISIG;
  param.c_iflag &= ~(IXON | IXOFF | IXANY);
  param.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL);  
  param.c_oflag &= ~OPOST;
  param.c_oflag &= ~ONLCR;
  cfsetispeed(&param, B115200);
  cfsetospeed(&param, B115200);
  
  if(tcsetattr(fd, TCSANOW, &param) < 0) {
    fprintf(stderr, "Status set failed.\n");
    exit(1);
  }
}

static gboolean update_data(void *asd) {
  
  char buf[512];
  int len;
  
  do {
    if((len = read_untilcr(buf, sizeof(buf))) < 0) {
      fprintf(stderr, "Read error.\n");
      exit(1);
    }
  } while (buf[0] == '#');
  sscanf(buf, "%*d, %le, %le, %*d", &cur_temp, &cur_hum);  

  sprintf(buf, "<span color='blue'>%2.1lf C</span>", cur_temp);
  gtk_label_set_markup(GTK_LABEL(temp), buf);
  sprintf(buf, "<span color='blue'>%2.2lf %%</span>", cur_hum);
  gtk_label_set_markup(GTK_LABEL(hum), buf);

  return 1;
}

int main(int argc, char *argv[]) {

  GtkWidget *window;
  GtkWidget *temp_label, *hum_label, *hbox1, *hbox2, *vbox;

  gtk_init();
  window = gtk_window_new();
  gtk_window_set_title(GTK_WINDOW(window), "Temperature & Humidty");
  gtk_window_set_default_size(GTK_WINDOW(window), 150, 40);

  temp_label = gtk_label_new("Temperature:");
  gtk_label_set_xalign(GTK_LABEL(temp_label), 0.0); // 0 = left, 1 = right

  hum_label = gtk_label_new("Humidity:");
  gtk_label_set_xalign(GTK_LABEL(hum_label), 0.0); // 0 = left, 1 = right

  temp = gtk_label_new("-----");
  gtk_widget_set_hexpand(GTK_WIDGET(temp), TRUE);
  gtk_label_set_xalign(GTK_LABEL(temp), 0.5);

  hum = gtk_label_new("-----");
  gtk_widget_set_hexpand(GTK_WIDGET(hum), TRUE);
  gtk_label_set_xalign(GTK_LABEL(hum), 0.5);

  hbox1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
  hbox2 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);

  gtk_box_append(GTK_BOX(hbox1), temp_label);
  gtk_box_append(GTK_BOX(hbox1), temp);

  gtk_box_append(GTK_BOX(hbox2), hum_label);
  gtk_box_append(GTK_BOX(hbox2), hum);

  gtk_box_append(GTK_BOX(vbox), hbox1);
  gtk_box_append(GTK_BOX(vbox), hbox2);

  gtk_window_set_child(GTK_WINDOW(window), vbox);

  if((fd = open(DEVICE, O_RDWR)) < 0) {
    fprintf(stderr, "Can't open device.\n");
    exit(1);
  }
  set_tty();

  g_timeout_add(UPDATE, update_data, NULL);

  gtk_widget_set_visible(window, TRUE);

  while (g_list_model_get_n_items(gtk_window_get_toplevels()) > 0) // Main iteration loop
    g_main_context_iteration(NULL, TRUE);

  close(fd);
  return 0;
}
