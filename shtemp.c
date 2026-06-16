/* 
 * Read temperature & humidity data from Adafruit SHT41 USB device and produce simple graphs.
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <gtk/gtk.h>
#include <cairo/cairo.h>

#define DEVICE "/dev/serial/by-id/usb-Adafruit_SHT4x_Trinkey_M0_C2CD0A4F503059384B2E3120FF0E230D-if00"

#define NPTS 1444
#define WIDTH 250
#define HEIGHT 200

#define MIN_TEMP 0.0
#define MAX_TEMP 50.0
#define TEMP_STEP 5.0
#define MIN_HUM 0.0
#define MAX_HUM 100.0
#define HUM_STEP 10.0

#define MAX_TEMP_WARN 40.0  // in C or F depending on if FAHRENHEIT is set or not
#define MIN_TEMP_WARN 10.0
#define MAX_HUM_WARN 60.0
#define MIN_HUM_WARN 0.0

// #define FAHRENHEIT // Define this for F degrees - the default is C

// Update every 1 minute -- NPTS chosen such that the display is for 24h
#define UPDATE (1 * 60)

int fd, n_data = 0;
double cur_temp = -1.0, cur_hum = -1.0, temps[NPTS], hums[NPTS];
GtkWidget *temp, *hum, *draw_temp, *draw_hum;
cairo_surface_t *surface_temp, *surface_hum;
cairo_t *cairo_temp, *cairo_hum;

void convert_data(double *orig, double *new_x, double *new_y, double min, double max) {

  int i;

  for (i = 0; i < n_data; i++) {
    new_x[i] = ((double) WIDTH) * ((double) i) / (double) NPTS;
    if(orig[i] < min) orig[i] = min;
    if(orig[i] > max) orig[i] = max;
    new_y[i] = ((double) HEIGHT) * (1.0 - ((orig[i] - min) / (max - min)));
  }
}

double find_min(double *data) {

  int i;
  double m = 1E99;

  for (i = 0; i < n_data; i++)
    if(data[i] < m) m = data[i];
  return m;
}

double find_max(double *data) {

  int i;
  double m = -1E99;

  for (i = 0; i < n_data; i++)
    if(data[i] > m) m = data[i];
  return m;
}

void func_draw(GtkDrawingArea *area, cairo_t *cairo, int width, int height, gpointer user_data) {

  int i, j;
  double data_x[NPTS], data_y[NPTS], *data;
  char buf[32];

  cairo_set_source_rgb(cairo, 1.0, 1.0, 1.0);
  cairo_paint(cairo);

  cairo_set_source_rgb(cairo, 0.0, 0.0, 0.0);
  cairo_set_line_width(cairo, 1.0);

  if(user_data == (void *) temps) 
    i = ((double) HEIGHT) * TEMP_STEP / (MAX_TEMP - MIN_TEMP);
  else
    i = ((double) HEIGHT) * HUM_STEP / (MAX_HUM - MIN_HUM);
  for(j = 0; j <= height; j += i) {
    cairo_move_to(cairo, 0, j);
    cairo_line_to(cairo, width, j);
  }
  cairo_stroke(cairo);
  
  cairo_select_font_face(cairo, "serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
  cairo_set_font_size(cairo, 14.0);
  cairo_set_source_rgb(cairo, 0.0, 0.0, 1.0);
  if(user_data == (void *) temps) {
    cairo_move_to(cairo, 5.0, 11.0);
    sprintf(buf, "%.0f", MAX_TEMP);
    cairo_show_text(cairo, buf);
    cairo_move_to(cairo, 5.0, HEIGHT - 3.0);
    sprintf(buf, "%.0f", MIN_TEMP);
    cairo_show_text(cairo, buf);
  } else {
    cairo_move_to(cairo, 5.0, 11.0);
    sprintf(buf, "%.0f", MAX_HUM);
    cairo_show_text(cairo, buf);
    cairo_move_to(cairo, 5.0, HEIGHT - 3.0);
    sprintf(buf, "%.0f", MIN_HUM);
    cairo_show_text(cairo, buf);
    cairo_move_to(cairo, WIDTH-30.0, HEIGHT-3.0);
    sprintf(buf, "%.0fh", (UPDATE / (60.0 * 60.0)) * (double) NPTS);
    cairo_show_text(cairo, buf);
  }

  cairo_set_source_rgb(cairo, 1.0, 0.0, 0.0);
  cairo_set_line_width(cairo, 2.0);
  cairo_set_line_join(cairo, CAIRO_LINE_JOIN_ROUND);

  if(n_data > 0) {
    if(user_data == (void *) temps)
      convert_data(temps, data_x, data_y, MIN_TEMP, MAX_TEMP);
    else
      convert_data(hums, data_x, data_y, MIN_HUM, MAX_HUM);
    cairo_move_to(cairo, data_x[0], data_y[0]);
    for(i = 1; i < n_data; i++)
      cairo_line_to(cairo, data_x[i], data_y[i]);
    cairo_stroke_preserve(cairo);    
  }
  gtk_widget_queue_draw(GTK_WIDGET(area));
}

int read_untilcr(char *buf, int maxlen) {
  
  int pos = 0, bytes_rd;
  
  tcflush(fd, TCIOFLUSH);
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
#ifdef FAHRENHEIT
  cur_temp = cur_temp * (9.0/5.0) + 32.0;
#endif

  if(n_data < NPTS) {
    temps[n_data] = cur_temp;
    hums[n_data] = cur_hum;
    n_data++;
  } else {
    int i;
    for (i = 0; i < NPTS-1; i++) {
      temps[i] = temps[i+1];
      hums[i] = hums[i+1];
    }
    temps[NPTS-1] = cur_temp;
    hums[NPTS-1] = cur_hum;
  }

#ifdef FAHRENHEIT
  if(cur_temp < MIN_TEMP_WARN || cur_temp > MAX_TEMP_WARN) 
    sprintf(buf, "<span color='red'>%2.0lf F (%2.0lf F, %2.0lf F)</span>", cur_temp, find_max(temps), find_min(temps));
  else
    sprintf(buf, "<span color='blue'>%2.0lf F (%2.0lf F, %2.0lf F)</span>", cur_temp, find_max(temps), find_min(temps));
#else
  if(cur_temp < MIN_TEMP_WARN || cur_temp > MAX_TEMP_WARN) 
    sprintf(buf, "<span color='red'>%2.0lf C (%2.0lf C, %2.0lf C)</span>", cur_temp, find_max(temps), find_min(temps));
  else
    sprintf(buf, "<span color='blue'>%2.0lf C (%2.0lf C, %2.0lf C)</span>", cur_temp, find_max(temps), find_min(temps));
#endif
  gtk_label_set_markup(GTK_LABEL(temp), buf);
  if(cur_hum < MIN_HUM_WARN || cur_hum > MAX_HUM_WARN)  
    sprintf(buf, "<span color='red'>%2.0lf %% (%2.0lf %%, %2.0lf %%)</span>", cur_hum, find_max(hums), find_min(hums));
  else
    sprintf(buf, "<span color='blue'>%2.0lf %% (%2.0lf %%, %2.0lf %%)</span>", cur_hum, find_max(hums), find_min(hums));
  gtk_label_set_markup(GTK_LABEL(hum), buf);

  func_draw(GTK_DRAWING_AREA(draw_temp), cairo_temp, 200, 200, (void *) temps);
  func_draw(GTK_DRAWING_AREA(draw_hum), cairo_hum, 200, 200, (void *) hums);

  return 1;
}

int main(int argc, char *argv[]) {

  GtkWidget *window;
  GtkWidget *temp_label, *hum_label, *hbox1, *hbox2, *vbox, *separ1, *separ2;

  gtk_init();
  window = gtk_window_new();
  gtk_window_set_title(GTK_WINDOW(window), "Temperature & Humidty");
  gtk_window_set_default_size(GTK_WINDOW(window), WIDTH, 50 + 2 * HEIGHT);

  temp_label = gtk_label_new("Temperature:");
  gtk_label_set_xalign(GTK_LABEL(temp_label), 0.0); // 0 = left, 1 = right

  hum_label = gtk_label_new("Humidity:");
  gtk_label_set_xalign(GTK_LABEL(hum_label), 0.0); // 0 = left, 1 = right

  temp = gtk_label_new("-----");
  gtk_widget_set_hexpand(GTK_WIDGET(temp), TRUE);
  gtk_label_set_xalign(GTK_LABEL(temp), 0.5);
  draw_temp = gtk_drawing_area_new();  
  gtk_widget_set_size_request(draw_temp, WIDTH, HEIGHT);
  gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(draw_temp), func_draw, (void *) temps, NULL);
  surface_temp = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, WIDTH, HEIGHT);
  cairo_temp = cairo_create(surface_temp);

  hum = gtk_label_new("-----");
  gtk_widget_set_hexpand(GTK_WIDGET(hum), TRUE);
  gtk_label_set_xalign(GTK_LABEL(hum), 0.5);
  draw_hum = gtk_drawing_area_new();  
  gtk_widget_set_size_request(draw_hum, WIDTH, HEIGHT);
  gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(draw_hum), func_draw, (void *) hums, NULL);

  surface_hum = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, WIDTH, HEIGHT);
  cairo_hum = cairo_create(surface_hum);

  hbox1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
  hbox2 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
  vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
  separ1 = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
  separ2 = gtk_separator_new(GTK_ORIENTATION_VERTICAL);

  gtk_box_append(GTK_BOX(hbox1), temp_label);
  gtk_box_append(GTK_BOX(hbox1), temp);

  gtk_box_append(GTK_BOX(hbox2), hum_label);
  gtk_box_append(GTK_BOX(hbox2), hum);

  gtk_box_append(GTK_BOX(vbox), hbox1);
  gtk_box_append(GTK_BOX(vbox), hbox2);
  gtk_box_append(GTK_BOX(vbox), separ1);
  gtk_box_append(GTK_BOX(vbox), draw_temp);
  gtk_box_append(GTK_BOX(vbox), separ2);
  gtk_box_append(GTK_BOX(vbox), draw_hum);

  gtk_window_set_child(GTK_WINDOW(window), vbox);

  if((fd = open(DEVICE, O_RDWR)) < 0) {
    fprintf(stderr, "Can't open device.\n");
    exit(1);
  }
  set_tty();

  update_data(NULL);
  g_timeout_add_seconds(UPDATE, update_data, NULL);

  gtk_widget_set_visible(window, TRUE);

  while (g_list_model_get_n_items(gtk_window_get_toplevels()) > 0) // Main iteration loop
    g_main_context_iteration(NULL, TRUE);

  close(fd);
  cairo_destroy(cairo_temp);
  cairo_destroy(cairo_hum);
  cairo_surface_destroy(surface_temp);
  cairo_surface_destroy(surface_hum);

  return 0;
}
