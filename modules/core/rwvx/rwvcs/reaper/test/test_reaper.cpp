
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 *
 */

#include <pthread.h>
#include <stdio.h>
#include <sys/un.h>
#include <sys/socket.h>

#include <libreaper.h>
#include <reaper_client.h>
#include <rwut.h>

struct two_clients_cls {
  char * path;
  uint16_t pid;
};

void on_disconnect(struct reaper * reaper, struct client * client) {
  reaper->stop = true;
}

void two_clients_on_disconnect(struct reaper * reaper, struct client * client) {
  static int seen = 0;

  seen++;
  if (seen == 2)
    reaper->stop = true;
}

static void * write_thread(void * arg) {
  int r;
  int sock;
  char * path;

  path = (char *)arg;

  sock = reaper_client_connect(path, NULL);
  EXPECT_GT(sock, 0);

  r = reaper_client_add_pid(sock, 1);
  EXPECT_EQ(0, r);

  r = reaper_client_add_pid(sock, 1839);
  EXPECT_EQ(0, r);

  r = reaper_client_add_pid(sock, 65535);
  EXPECT_EQ(0, r);

  r = reaper_client_del_pid(sock, 65535);
  EXPECT_EQ(0, r);

  r = reaper_client_del_pid(sock, 1);
  EXPECT_EQ(0, r);

  r = reaper_client_del_pid(sock, 1839);
  EXPECT_EQ(0, r);

  r = reaper_client_add_pid(sock, 1);
  EXPECT_EQ(0, r);

  r = reaper_client_add_pid(sock, 1839);
  EXPECT_EQ(0, r);

  r = reaper_client_add_pid(sock, 65535);
  EXPECT_EQ(0, r);

  r = reaper_client_add_path(sock, "path1");
  EXPECT_EQ(0, r);

  r = reaper_client_add_path(sock, "path2");
  EXPECT_EQ(0, r);

  reaper_client_disconnect(sock);

  return NULL;
}

static void * two_clients_thread(void * arg) {
  int r;
  int sock;
  struct two_clients_cls * cls;

  cls = (struct two_clients_cls *)arg;

  sock = reaper_client_connect(cls->path, NULL);
  EXPECT_GT(sock, 0);

  r = reaper_client_add_pid(sock, cls->pid);
  EXPECT_EQ(0, r);

  reaper_client_disconnect(sock);

  return NULL;
}

class ReaperTest: public ::testing::Test {
  private:
    struct reaper * m_reaper;
    char * m_path;

  protected:
    virtual void SetUp() {
      m_path = tmpnam(NULL);
      //m_path = strdup("/tmp/reaper-test");
      ASSERT_TRUE(m_path);
      fprintf(stderr, "Creating socket at %s\n", m_path);

      m_reaper = reaper_alloc(m_path, &on_disconnect);
    }

    virtual void TearDown() {
      reaper_free(&m_reaper);
      unlink(m_path);
    }

    virtual void TestAddClient() {
      int r;
      bool found = false;
      struct client * cp;

      r = reaper_add_client(m_reaper, 10);
      ASSERT_FALSE(r);

      SLIST_FOREACH(cp, &m_reaper->clients, slist) {
        if (cp->socket == 10) {
          found = true;
          break;
        }
      }

      ASSERT_TRUE(found);
    }

    virtual void TestAddClientPid() {
      int r;
      bool found = false;
      struct client * cp;

      r = reaper_add_client(m_reaper, 10);
      ASSERT_FALSE(r);

      SLIST_FOREACH(cp, &m_reaper->clients, slist) {
        if (cp->socket == 10)
          break;
      }

      r = reaper_add_client_pid(m_reaper, cp, 1);
      ASSERT_FALSE(r);

      r = reaper_add_client_pid(m_reaper, cp, 200);
      ASSERT_FALSE(r);

      r = reaper_add_client_pid(m_reaper, cp, 65535);
      ASSERT_FALSE(r);

      SLIST_FOREACH(cp, &m_reaper->clients, slist) {
        if (cp->socket == 10) {
          int n_found = 0;
          found = true;

          for (size_t i = 0; cp->pids[i]; ++i) {
            if (cp->pids[i] == 1
                || cp->pids[i] == 200
                || cp->pids[i] == 65535)
              ++n_found;
          }

          ASSERT_EQ(n_found, 3);
          break;
        }
      }

      ASSERT_TRUE(found);

      found = false;

      r = reaper_del_client_pid(m_reaper, cp, 65535);
      ASSERT_FALSE(r);

      r = reaper_del_client_pid(m_reaper, cp, 1);
      ASSERT_FALSE(r);

      r = reaper_del_client_pid(m_reaper, cp, 200);
      ASSERT_FALSE(r);

      SLIST_FOREACH(cp, &m_reaper->clients, slist) {
        if (cp->socket == 10) {
          int n_found = 0;
          found = true;

          for (size_t i = 0; cp->pids[i]; ++i) {
            if (cp->pids[i] == 1
                || cp->pids[i] == 200
                || cp->pids[i] == 65535)
              ++n_found;
          }

          ASSERT_EQ(n_found, 0);
          break;
        }
      }

      ASSERT_TRUE(found);
    }

    virtual void TestAddClientPath() {
      int r;
      bool found = false;
      struct client * cp;

      r = reaper_add_client(m_reaper, 10);
      ASSERT_FALSE(r);

      SLIST_FOREACH(cp, &m_reaper->clients, slist) {
        if (cp->socket == 10)
          break;
      }

      r = reaper_add_client_path(m_reaper, cp, "path1");
      ASSERT_FALSE(r);

      r = reaper_add_client_path(m_reaper, cp, "path2");
      ASSERT_FALSE(r);

      SLIST_FOREACH(cp, &m_reaper->clients, slist) {
        if (cp->socket == 10) {
          int n_found = 0;
          found = true;

          for (size_t i = 0; cp->paths[i]; ++i) {
            if (!strcmp(cp->paths[i], "path1")
                || !strcmp(cp->paths[i], "path2"))
              ++n_found;
          }

          ASSERT_EQ(n_found, 2);
          break;
        }
      }

      ASSERT_TRUE(found);
    }


    virtual void TestLoop() {
      int r;
      pthread_t tid;
      struct client * cp;
      size_t clients = 0;

      r = pthread_create(&tid, NULL, &write_thread, m_path);
      ASSERT_FALSE(r);

      reaper_loop(m_reaper);

      SLIST_FOREACH(cp, &m_reaper->clients, slist) {
        ++clients;
      }
      ASSERT_EQ(clients, 1);

      SLIST_FOREACH(cp, &m_reaper->clients, slist) {
        int n_found = 0;

        for (size_t i = 0; cp->pids[i]; ++i) {
          if (cp->pids[i] == 1
              || cp->pids[i] == 1839
              || cp->pids[i] == 65535)
            ++n_found;
        }
        ASSERT_EQ(n_found, 3);

        n_found = 0;
        for (size_t i = 0; cp->paths[i]; ++i) {
          if (!strcmp(cp->paths[i], "path1")
              || !strcmp(cp->paths[i], "path2"))
            ++n_found;
        }
        ASSERT_EQ(n_found, 2);
      }
    }

    virtual void TestTwoClients() {
      int r;
      pthread_t threads[2];
      struct client * cp;
      size_t clients = 0;
      bool found_client_pids[2] = {false, false};
      struct two_clients_cls cls[2];

      cls[0].path = m_path;
      cls[0].pid = 1;
      cls[1].path = m_path;
      cls[1].pid = 2;

      reaper_free(&m_reaper);
      m_reaper = reaper_alloc(m_path, &two_clients_on_disconnect);

      for (size_t i = 0; i < 2; ++i) {
        r = pthread_create(&threads[i], NULL, &two_clients_thread, &cls[i]);
        ASSERT_FALSE(r);
      }

      reaper_loop(m_reaper);

      SLIST_FOREACH(cp, &m_reaper->clients, slist) {
        ++clients;
      }
      ASSERT_EQ(clients, 2);

      SLIST_FOREACH(cp, &m_reaper->clients, slist) {
        for (size_t i = 0; cp->pids[i]; ++i) {
          switch (cp->pids[i]) {
            case 1:
              ASSERT_FALSE(found_client_pids[0]);
              found_client_pids[0] = true;
              break;
            case 2:
              ASSERT_FALSE(found_client_pids[1]);
              found_client_pids[1] = true;
              break;
            default:
              ASSERT_TRUE(false);
          }
        }
      }

      ASSERT_TRUE(found_client_pids[0]);
      ASSERT_TRUE(found_client_pids[1]);
    }

    virtual void TestFileExist() {
      reaper_free(&m_reaper);
      int fd, r;

      /* CREATE A REGULAR FILE AT m_path*/
      fd = open (m_path, O_RDWR|O_CREAT);
      ASSERT_TRUE(fd);

      /* TRY reaper_alloc; IT SHALL FAIL*/
      m_reaper = reaper_alloc(m_path, &on_disconnect);
      ASSERT_FALSE(m_reaper);
      unlink(m_path);

      fd = socket(AF_UNIX, SOCK_STREAM, 0);
      ASSERT_TRUE(fd);
      struct sockaddr_un addr;
      bzero(&addr, sizeof(struct sockaddr_un));
      addr.sun_family = AF_UNIX;
      r = snprintf(addr.sun_path, UNIX_PATH_MAX, m_path);
      ASSERT_TRUE(r);
      r = bind(fd, (struct sockaddr *)&addr, sizeof(struct sockaddr_un) -1);
      ASSERT_FALSE(r);

      /* TRY reaper_alloc; THIS SHALL SUCCEED */
      m_reaper = reaper_alloc(m_path, &on_disconnect);
      ASSERT_TRUE(m_reaper);

    }
};

TEST_F(ReaperTest, TestAddClient) {
  TestAddClient();
}

TEST_F(ReaperTest, TestAddClientPid) {
  TestAddClientPid();
}

TEST_F(ReaperTest, TestAddClientPaths) {
  TestAddClientPid();
}

TEST_F(ReaperTest, TestLoop) {
  TestLoop();
}

TEST_F(ReaperTest, TestTwoClients) {
  TestTwoClients();
}

TEST_F(ReaperTest, TestFileExist) {
  TestFileExist();
}

