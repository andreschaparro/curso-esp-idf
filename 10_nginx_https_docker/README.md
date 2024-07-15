# Capitulo 10: Ngnix con Https y Docker

## Crear el archivo nginx.conf de un webserver estatico con HTTPS utilizando Ngnix

1. Ejecutar `cd iot-platform`.
2. Ejecutar `mkdir nginx`.
3. Ejecutar `cd nginx`.
4. Ejecutar `mkdir config`.
5. Ejecutar `cd config`.
6. Ejecutar `touch nginx.conf`.
7. Modificar el contenido de `nginx.conf`:

```
user  nginx;
worker_processes  auto;

error_log  /var/log/nginx/error.log notice;
pid        /var/run/nginx.pid;


events {
    worker_connections  1024;
}


http {
    include       /etc/nginx/mime.types;
    default_type  application/octet-stream;

    log_format  main  '$remote_addr - $remote_user [$time_local] "$request" '
                      '$status $body_bytes_sent "$http_referer" '
                      '"$http_user_agent" "$http_x_forwarded_for"';

    access_log  /var/log/nginx/access.log  main;

    sendfile        on;
    #tcp_nopush     on;

    keepalive_timeout  65;

    #gzip  on;

    #HTTP server
    #server {
    #    listen 8080;
    #    server_name localhost;
    #
    #    location / {
    #        root /usr/share/nginx/html;
    #    }
    #
    #}

    # HTTPS server
    server {
        listen       8443 ssl;
        server_name  raspberry.local;

        ssl_certificate      /etc/nginx/certs/server.crt;
        ssl_certificate_key  /etc/nginx/certs/server.key;
        ssl_client_certificate  /etc/nginx/certs/ca.crt;
        ssl_verify_client on;

        ssl_protocols       TLSv1 TLSv1.1 TLSv1.2 TLSv1.3;
        ssl_ciphers         HIGH:!aNULL:!MD5;

        location / {
            root /usr/share/nginx/html;
        }
    }

    #include /etc/nginx/conf.d/*.conf;
}
```

En los siguientes sitios, esta la documentacion:

[Beginner’s Guide](http://nginx.org/en/docs/beginners_guide.html)

[Configuring HTTPS servers](http://nginx.org/en/docs/http/configuring_https_servers.html)

## Reutilizar los certificados y llaves TLS para HTTPS

1. Ejecutar `cd ..`.
2. Ejecutar `cd ..`.
3. Ejecutar `cd certs`.
4. Modificar el contenido de `crearTLS.sh`:

```
#!/bin/bash

HOSTNAME="raspberrypi.local"
SUBJECT_CA="/C=AR/ST=AMBA/L=Municipio/O=Empresa/OU=CA/CN=$HOSTNAME"
SUBJECT_SERVER="/C=AR/ST=AMBA/L=Municipio/O=Empresa/OU=Server/CN=$HOSTNAME"
SUBJECT_CLIENT="/C=AR/ST=AMBA/L=Municipio/O=Empresa/OU=Client/CN=$HOSTNAME"

function generate_CA () {
    echo "$SUBJECT_CA"
    openssl req -x509 -nodes -sha256 -newkey rsa:2048 -subj "$SUBJECT_CA"  -days 825 -keyout ca.key -out ca.crt
    openssl x509 -in ca.crt -text -noout
}

function generate_server () {
    echo "$SUBJECT_SERVER"
    openssl req -nodes -sha256 -new -subj "$SUBJECT_SERVER" -keyout server.key -out server.csr
    openssl x509 -req -sha256 -in server.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out server.crt -days 825
    openssl x509 -in server.crt -text -noout
    openssl verify -CAfile ca.crt server.crt
}

function generate_client () {
    echo "$SUBJECT_CLIENT"
    openssl req -new -nodes -sha256 -subj "$SUBJECT_CLIENT" -out client.csr -keyout client.key
    openssl x509 -req -sha256 -in client.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out client.crt -days 825
    openssl x509 -in client.crt -text -noout
    openssl verify -CAfile ca.crt client.crt
}

function copy_to_mosquitto() {
    mkdir ../mosquitto/certs
    cp server.crt ../mosquitto/certs
    cp server.key ../mosquitto/certs
    cp ca.crt ../mosquitto/certs
}

function copy_to_nginx() {
    mkdir ../nginx/certs
    cp server.crt ../nginx/certs
    cp server.key ../nginx/certs
    cp ca.crt ../nginx/certs
}

# generate_CA
# generate_server
# generate_client
# copy_to_mosquitto
copy_to_nginx
```

5. Ejecutar `./crearTLS.sh`.
6. Modificar el contenido de `crearTLS.sh`:

```
#!/bin/bash

HOSTNAME="raspberrypi.local"
SUBJECT_CA="/C=AR/ST=AMBA/L=Municipio/O=Empresa/OU=CA/CN=$HOSTNAME"
SUBJECT_SERVER="/C=AR/ST=AMBA/L=Municipio/O=Empresa/OU=Server/CN=$HOSTNAME"
SUBJECT_CLIENT="/C=AR/ST=AMBA/L=Municipio/O=Empresa/OU=Client/CN=$HOSTNAME"

function generate_CA () {
    echo "$SUBJECT_CA"
    openssl req -x509 -nodes -sha256 -newkey rsa:2048 -subj "$SUBJECT_CA"  -days 825 -keyout ca.key -out ca.crt
    openssl x509 -in ca.crt -text -noout
}

function generate_server () {
    echo "$SUBJECT_SERVER"
    openssl req -nodes -sha256 -new -subj "$SUBJECT_SERVER" -keyout server.key -out server.csr
    openssl x509 -req -sha256 -in server.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out server.crt -days 825
    openssl x509 -in server.crt -text -noout
    openssl verify -CAfile ca.crt server.crt
}

function generate_client () {
    echo "$SUBJECT_CLIENT"
    openssl req -new -nodes -sha256 -subj "$SUBJECT_CLIENT" -out client.csr -keyout client.key
    openssl x509 -req -sha256 -in client.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out client.crt -days 825
    openssl x509 -in client.crt -text -noout
    openssl verify -CAfile ca.crt client.crt
}

function copy_to_mosquitto() {
    mkdir ../mosquitto/certs
    cp server.crt ../mosquitto/certs
    cp server.key ../mosquitto/certs
    cp ca.crt ../mosquitto/certs
}

function copy_to_nginx() {
    mkdir ../nginx/certs
    cp server.crt ../nginx/certs
    cp server.key ../nginx/certs
    cp ca.crt ../nginx/certs
}

generate_CA
generate_server
generate_client
copy_to_mosquitto
copy_to_nginx
```

## Crear el archivo Dockerfile para generar la imagen de un webserver estatico con HTTPS utilizando Ngnix

1. Ejecutar `cd ..`.
2. Ejecutar `cd nginx`.
3. Ejecutar `touch Dockerfile`.
4. Modificar el contenido de `Dockerfile`:

```
FROM nginx:latest
COPY ./config/nginx.conf /etc/nginx/nginx.conf
COPY ./certs/server.crt /etc/nginx/certs/server.crt
COPY ./certs/server.key /etc/nginx/certs/server.key
COPY ./certs/ca.crt /etc/nginx/certs/ca.crt
COPY ./updates /usr/share/nginx/html/updates
EXPOSE 8443
```

En el siguiente sitio, esta la documentacion:

[Dockerfile reference](https://docs.docker.com/reference/dockerfile/)

## Subir un archivo al webserver estatico con HTTPS utilizando Ngnix

1. Ejecutar `mkdir updates`.
2. Copiar el archivo `1_hola_mundo.bin` que esta dentro la carpeta `build` del proyecto `1_hola_muindo` a la carpeta `updates`.

## Modificar el archivo compose.yaml para generar el contenedor de Nginx con HTTPS

1. Ejecutar `cd ..`.
2. Modificar el contenido de `compose.yaml`:

```
services:
  mosquitto:
    build: mosquitto
    container_name: mosquitto
    expose:
      - "8883"
    ports:
      - "8883:8883"

  nginx:
    build: nginx
    container_name: nginx
    expose:
      - "8443"
    ports:
      - "8443:8443"
```

3. Ejecutar `docker compose up --build`.

En el siguiente sitio, esta la documentacion:

[Docker Compose Quickstart](https://docs.docker.com/compose/gettingstarted/)

## Probar el webserver estatico con HTTPS utilizando Ngnix

1. Abrir en el navegador la ruta `https://raspberrypi.local:8443/updates/1_hola_mundo.bin`.

![Error](error.png)
