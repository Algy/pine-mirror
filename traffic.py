import socket
import select
import time
import errno

class TrafficConnectionError(Exception):
    pass


class SimpleTrafficClient(object):
    def connect(self, host="localhost", port=5123):
        self.sock = socket.socket()
        self.sock.connect((host, port))

    def _make_error(self, e):
        if isinstance(e.args, tuple):
            no = e.args[0]
        else:
            no = e.args
        if no == errno.EPIPE:
            raise TrafficConnectionError("Broken Pipe")
        else:
            raise TrafficConnectionError("IO Error %d"%no)

    def _one_char_simple_command(self, c):
        try:
            self.sock.sendall(c)
        except socket.error, e:
            self._make_error(e)
        buf = self.sock.recv(1)
        if not buf:
            raise TrafficConnectionError("Disconnected")
        return buf == 'Y'

    def wait(self):
        return self._one_char_simple_command('W')

    def try_acquire_lock(self):
        return self._one_char_simple_command('A')
    
    def release_lock(self):
        return self._one_char_simple_command('R')

    def close(self):
        if hasattr(self, "sock"):
            self.sock.close()


class SimpleTrafficServer(object):
    def __init__(self, delay):
        self.delay = delay

    def serve_forever(self, host="0.0.0.0", port=5123):
        server_sock = socket.socket()
        server_sock_fd = server_sock.fileno()
        server_sock.setblocking(0)
        server_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        server_sock.bind((host, port))
        server_sock.listen(5)

        client_socket_pool = {} # fd |-> socket

        waiting_fds = set()
        fds_given_chance = set()

        global_lock = [False]
        global_lock_owner_fd = [None]

        def send(fd, buf):
            try:
                client_socket_pool[fd].sendall(buf)
            except socket.error, e:
                if isinstance(e, tuple):
                    no = e.args[0]
                else:
                    no = e.args
                if no != errno.EPIPE:
                    print "[ERROR]socket error: %s"%(str(e))
                teardown_connection(fd)
                return False
            return True

        def try_acquire_lock(fd):
            if not global_lock[0]:
                global_lock[0] = True
                global_lock_owner_fd[0] = fd
                return True
            else:
                return False

        def release_lock():
            result = global_lock[0]
            global_lock[0] = False
            global_lock_owner_fd[0] = None
            return result

        def teardown_connection(fd):
            print "[INFO] Disconnect fd %d"%fd
            try:
                waiting_fds.remove(fd)
            except KeyError:
                pass
            try:
                fds_given_chance.remove(fd)
            except KeyError:
                pass
            client_socket_pool.pop(fd).close()
            # robust implementation of lock
            if fd == global_lock_owner_fd[0]:
                print "[WARN] fd: %d has disconnected without releasing lock"%fd
                release_lock()
        try:
            last_time = time.time()
            while True:
                rd_list, _, _ = select.select(list(client_socket_pool.keys()) + [server_sock_fd], [], [], 0.1)
                for rd_fd in rd_list:
                    if rd_fd == server_sock_fd: # accept
                        client_sock, (ip, _) = server_sock.accept()
                        print "[INFO] Accepted %s"%ip
                        client_socket_pool[client_sock.fileno()] = client_sock
                    elif rd_fd in client_socket_pool:
                        client_sock = client_socket_pool[rd_fd]
                        buf = client_sock.recv(1)
                        if not buf:
                            teardown_connection(rd_fd)
                        if buf == 'W':
                            waiting_fds.add(rd_fd)
                        elif buf == 'A':
                            print "[INFO] lock: %s ->"%(repr(global_lock)),
                            if try_acquire_lock(rd_fd):
                                send(rd_fd, 'Y')
                            else:
                                send(rd_fd, 'N')
                            print repr(global_lock)
                        elif buf == 'R':
                            print "[INFO] lock: %s ->"%(repr(global_lock)),
                            if release_lock():
                                send(rd_fd, 'Y')
                            else:
                                send(rd_fd, 'N')
                            print repr(global_lock)
                    else:
                        print "[ERROR]Invallid read fd: %d"%rd_fd

                if last_time + self.delay <= time.time():
                    # select one client not having been selected. 
                    # If all the clients have been given a chance, reset fds_given_chance
                    if waiting_fds:
                        try: 
                            chosen_fd = (fd for fd in waiting_fds 
                                         if fd not in fds_given_chance).next()
                        except StopIteration:
                            fds_given_chance.clear()
                            chosen_fd = iter(waiting_fds).next()
                        fds_given_chance.add(chosen_fd)
                        if send(chosen_fd, 'Y'):
                            waiting_fds.remove(chosen_fd)
                    last_time = time.time()
        finally:
            server_sock.close()
    

if __name__ == '__main__':
    host = "localhost"
    port = 5123
    delay = 1.5
    print "Serving on %s port %d...(delay %.2lf s)"%(host, port, delay)
    server = SimpleTrafficServer(delay)
    server.serve_forever(host, port)
