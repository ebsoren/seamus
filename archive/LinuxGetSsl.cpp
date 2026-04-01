#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <openssl/ssl.h>

#include <iostream>
#include <sstream>

class ParsedUrl
   {
   public:
      const char *CompleteUrl;
      char *Service, *Host, *Port, *Path;

      ParsedUrl( const char *url )
         {
         // Assumes url points to static text but
         // does not check.

         CompleteUrl = url;

         pathBuffer = new char[ strlen( url ) + 1 ];
         const char *f;
         char *t;
         for ( t = pathBuffer, f = url; *t++ = *f++; )
            ;

         Service = pathBuffer;

         const char Colon = ':', Slash = '/';
         char *p;
         for ( p = pathBuffer; *p && *p != Colon; p++ )
            ;

         if ( *p )
            {
            // Mark the end of the Service.
            *p++ = 0;

            if ( *p == Slash )
               p++;
            if ( *p == Slash )
               p++;

            Host = p;

            for ( ; *p && *p != Slash && *p != Colon; p++ )
               ;

            if ( *p == Colon )
               {
               // Port specified.  Skip over the colon and
               // the port number.
               *p++ = 0;
               Port = +p;
               for ( ; *p && *p != Slash; p++ )
                  ;
               }
            else
               Port = p;

            if ( *p )
               // Mark the end of the Host and Port.
               *p++ = 0;

            // Whatever remains is the Path.
            Path = p;
            }
         else
            Host = Path = p;
         }

      ~ParsedUrl( )
         {
         delete[ ] pathBuffer;
         }

   private:
      char *pathBuffer;
   };

int main( int argc, char **argv ) {

   if ( argc != 2 ) {
      std::cerr << "Usage: " << argv[ 0 ] << " url" << std::endl;
      return 1;
   }

   // Parse the URL
   ParsedUrl url( argv[ 1 ] );

   // Get the host address.
   struct addrinfo hints;
   struct addrinfo* res;

   memset(&hints, 0, sizeof(hints));
   hints.ai_family = AF_INET;
   hints.ai_socktype = SOCK_STREAM;
   hints.ai_protocol = IPPROTO_TCP;

   // Port 443 is HTTPS
   if (getaddrinfo(url.Host, "443", &hints, &res) != 0) {
      std::cerr << "getaddrinfo failed\n";
      return 1;
   }

   // Create a TCP/IP socket.
   int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

   // Connect the socket to the host address.
   if (connect(sock, res->ai_addr, res->ai_addrlen) != 0 ) {
      std::cerr << "connect failed: " << strerror(errno) << "\n";
      return 1;
   }

   // Build an SSL layer and set it to read/write
   // to the socket we've connected.

   // TODO: The HW assignment says all this stuff is out of date but it's what AG supports
   // For the actual project, update to use non-deprecated openSSL
   SSL_library_init();
   SSL_CTX* ssl_ctx = SSL_CTX_new(SSLv23_method());
   SSL* ssl = SSL_new(ssl_ctx);
   SSL_set_tlsext_host_name(ssl, url.Host); // HW didn't tell us to do this, but was needed to fix SNI error
   SSL_set_fd(ssl, sock);
   SSL_connect(ssl);

   // Send a GET message.
   std::stringstream ss;
   ss << "GET /" << url.Path <<  " HTTP/1.1\r\n";
   ss << "Host: " << url.Host << "\r\n";
   ss << "User-Agent: Seamus the Search Engine (web crawler for university course)\r\n";
   ss << "Accept: */*\r\n";
   ss << "Accept-Encoding: identity\r\n";
   ss << "Connection: close\r\n\r\n";

   std::string get(ss.str());
   int sent = 0;

   do {
      sent += SSL_write(ssl, get.c_str(), get.size() - sent);
   } while (sent < get.size());

   // Read from the socket until there's no more data, copying it to
   // stdout.
   int rcvd, total;
   char buff[10240];

   bool look_for_header = true;
   do {
      rcvd = SSL_read(ssl, buff, sizeof(buff));
      total += rcvd;

      if (rcvd > 0) {
         std::string response(buff, std::min(total, rcvd));
         size_t loc = response.find("\r\n\r\n");
         std::string output = loc == std::string::npos || (response.substr(0, 4) != "HTTP" && !look_for_header) ? response : response.substr(loc + 4);

         if (look_for_header && loc == std::string::npos && response.substr(0, 4) == "HTTP") continue;
         std::cout << output;
         // std::cout << response; // for testing
         
         look_for_header = false;
      }
   } while (rcvd > 0);

   // Close the socket and free the address info structure.
   // TODO: In actual project, RAII this
   SSL_shutdown(ssl);
   SSL_free(ssl);
   close(sock);
   SSL_CTX_free(ssl_ctx);
   freeaddrinfo(res);
}

