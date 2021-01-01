#include "ps4.h"

#include "main.h"

int nthread_run;
char notify_buf[512];
configuration config;

void *nthread_func(void *arg) {
  UNUSED(arg);
  time_t t1, t2;
  t1 = 0;
  while (nthread_run) {
    if (notify_buf[0]) {
      t2 = time(NULL);
      if ((t2 - t1) >= config.notify) {
        t1 = t2;
        systemMessage(notify_buf);
      }
    } else {
      t1 = 0;
    }
    sceKernelSleep(1);
  }

  return NULL;
}

static int config_handler(void *user, const char *name, const char *value) {
  configuration *pconfig = (configuration *)user;

#define MATCH(n) strcmp(name, n) == 0

  if (MATCH("split")) {
    pconfig->split = atoi(value);
  } else if (MATCH("notify")) {
    pconfig->notify = atoi(value);
  } else if (MATCH("shutdown")) {
    pconfig->shutdown = atoi(value);
  };

  return 1;
}

int npbind_parse(const char *filename) {
  struct npbind_header header;
  memset_s(&header, sizeof(struct npbind_header), 0, sizeof(struct npbind_header));

  int fd = open (filename, O_RDONLY, 0);
  if (fd < 0)  {
    return 1;
  }

  if (sizeof(struct npbind_header) != read(fd, &header, sizeof(struct npbind_header))) {
    close(fd);
    return 2;
  }

  header.magic = bswap32(header.magic);
  header.version = bswap32(header.version);
  header.file_size = bswap64(header.file_size);
  header.entry_size = bswap64(header.entry_size);
  header.num_entries = bswap64(header.num_entries);
  // DEBUG: printf_notification("Magic: 0x%08X\nVersion: %u\nFileize: %u bytes\nEntry Size: %u bytes\nNum of Entries: %u", header.magic, header.version, header.file_size, header.entry_size, header.num_entries);

  if (header.magic != NPBIND_MAGIC) {
    close(fd);
    return 3;
  }

  // --- HEADER BUILT AND CORRECT ENDIAN SET ----------------------------------

  // 0x14 is the size of the digest that is tacked onto the end of the file
  int num_bodies = (header.file_size - sizeof(struct npbind_header) - 0x14) /  header.entry_size;

  struct npbind_body body;
  memset_s(&body, header.entry_size, 0, header.entry_size);
  while((int64_t)header.entry_size == read(fd, &body, header.entry_size)) {
    printf_notification("%s", body.npcommid_name.data);
  }

  close(fd);
  return 0;
}

void dump_app(char *title_id, char *usb_path) {
  char base_path[64] = {0};
  char src_path[64] = {0};
  char dst_file[64] = {0};
  char dst_app[64] = {0};
  char dst_pat[64] = {0};
  char dump_sem[64] = {0};
  char comp_sem[64] = {0};

  sprintf(base_path, "%s/%s", usb_path, title_id);

  sprintf(dump_sem, "%s.dumping", base_path);
  sprintf(comp_sem, "%s.complete", base_path);

  unlink(comp_sem);
  touch_file(dump_sem);

  if (config.split) {
    sprintf(dst_app, "%s-app", base_path);
    sprintf(dst_pat, "%s-patch", base_path);
    if (config.split & SPLIT_APP) {
      mkdir(dst_app, 0777);
    }
    if (config.split & SPLIT_PATCH) {
      mkdir(dst_pat, 0777);
    }
  } else {
    sprintf(dst_app, "%s", base_path);
    sprintf(dst_pat, "%s", base_path);
    mkdir(base_path, 0777);
  }

  if ((!config.split) || (config.split & SPLIT_APP)) {
    sprintf(src_path, "/user/app/%s/app.pkg", title_id);
    printf_notification("Extracting app package...");
    unpkg(src_path, dst_app);
    sprintf(src_path, "/system_data/priv/appmeta/%s/nptitle.dat", title_id);
    sprintf(dst_file, "%s/sce_sys/nptitle.dat", dst_app);
    copy_file(src_path, dst_file);
    sprintf(src_path, "/system_data/priv/appmeta/%s/npbind.dat", title_id);
    sprintf(dst_file, "%s/sce_sys/npbind.dat", dst_app);
    copy_file(src_path, dst_file);
  }

  if ((!config.split) || (config.split & SPLIT_PATCH)) {
    sprintf(src_path, "/user/patch/%s/patch.pkg", title_id);
    if (file_exists(src_path)) {
      if (config.split) {
        printf_notification("Extracting patch package...");
      } else {
        printf_notification("Merging patch package...");
      }
      unpkg(src_path, dst_pat);
      sprintf(src_path, "/system_data/priv/appmeta/%s/nptitle.dat", title_id);
      sprintf(dst_file, "%s/sce_sys/nptitle.dat", dst_pat);
      copy_file(src_path, dst_file);
      sprintf(src_path, "/system_data/priv/appmeta/%s/npbind.dat", title_id);
      sprintf(dst_file, "%s/sce_sys/npbind.dat", dst_pat);
      copy_file(src_path, dst_file);
    }
  }

  if ((!config.split) || (config.split & SPLIT_APP)) {
    sprintf(src_path, "/mnt/sandbox/pfsmnt/%s-app0-nest/pfs_image.dat", title_id);
    printf_notification("Extracting app image...");
    unpfs(src_path, dst_app);
  }

  if ((!config.split) || (config.split & SPLIT_PATCH)) {
    sprintf(src_path, "/mnt/sandbox/pfsmnt/%s-patch0-nest/pfs_image.dat", title_id);
    if (file_exists(src_path)) {
      if (config.split) {
        printf_notification("Extracting patch image...");
      } else {
        printf_notification("Applying patch...");
      }
      unpfs(src_path, dst_pat);
    }
  }

  if ((!config.split) || (config.split & SPLIT_APP)) {
    sprintf(src_path, "/mnt/sandbox/pfsmnt/%s-app0", title_id);
    printf_notification("Decrypting selfs...");
    decrypt_dir(src_path, dst_app);
  }

  if ((!config.split) || (config.split & SPLIT_PATCH)) {
    sprintf(src_path, "/mnt/sandbox/pfsmnt/%s-patch0", title_id);
    if (file_exists(src_path)) {
      printf_notification("Decrypting patch...");
      decrypt_dir(src_path, dst_pat);
    }
  }

  unlink(dump_sem);
  touch_file(comp_sem);
}

int _main(struct thread *td) {
  UNUSED(td);

  char title_id[64] = {0};
  char usb_name[64] = {0};
  char usb_path[64] = {0};
  char cfg_path[64] = {0};

  initKernel();
  initLibc();
  initPthread();

  jailbreak();

  initSysUtil();

  config.split = 3;
  config.notify = 60;
  config.shutdown = 0;

  nthread_run = 1;
  notify_buf[0] = '\0';
  ScePthread nthread;
  scePthreadCreate(&nthread, NULL, nthread_func, NULL, "nthread");

  printf_notification("Running App Dumper");
  sceKernelSleep(5);

  if (!wait_for_usb(usb_name, usb_path)) {
    sprintf(notify_buf, "Waiting for USB device...");
    do {
      sceKernelSleep(1);
    } while (!wait_for_usb(usb_name, usb_path));
    notify_buf[0] = '\0';
  }

  sprintf(cfg_path, "%s/dumper.cfg", usb_path);
  cfg_parse(cfg_path, config_handler, &config);

  if (!wait_for_app(title_id)) {
    sprintf(notify_buf, "Waiting for application to launch...");
    do {
      sceKernelSleep(1);
    } while (!wait_for_app(title_id));
    notify_buf[0] = '\0';
  }

  // DEBUG: char npbindpath[64] = {0};
  // DEBUG: sprintf(npbindpath, "/system_data/priv/appmeta/%s/npbind.dat", title_id);
  // DEBUG: npbind_parse(npbindpath);

  if (wait_for_bdcopy(title_id) < 100) {
    int progress;
    do {
      sceKernelSleep(1);
      progress = wait_for_bdcopy(title_id);
      sprintf(notify_buf, "Waiting for application to copy\n%i%% completed...", progress);
    } while (progress < 100);
    notify_buf[0] = '\0';
  }

  printf_notification("Start dumping\n%s to %s", title_id, usb_name);
  sceKernelSleep(5);

  dump_app(title_id, usb_path);

  if (config.shutdown) {
    printf_notification("%s dumped.\nShutting down...", title_id);
  } else {
    printf_notification("%s dumped.\nQuitting...", title_id);
  }

  nthread_run = 0;

  if (config.shutdown) {
    sceKernelSleep(10);
    reboot();
  }

  return 0;
}
