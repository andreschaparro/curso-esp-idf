# Capitulo 2: Aplicacion que utliza un pulsador

## Components externos

Hay varios sitios donde podemos obtener otros components, que no vienen con el ESP-IDF, y que nos facilitaran el desarrollo de nuestras aplicaciones.

El primer sitio es:

[framework ESP-IoT-Solution](https://docs.espressif.com/projects/esp-iot-solution/en/latest/index.html)

Cuyos componentes se podran descargar del:

[ESP Component Registry](https://components.espressif.com/)

El segundo sitio es:

[UncleRus](https://github.com/UncleRus/esp-idf-lib)

## Agregar un component que esta en el framework ESP-IoT-Solution

1. Abrir la pagina [ESP Component Registry](https://components.espressif.com/).
2. Buscar el component.
3. Abrir su pagina.
4. Copiar el comando que esta bajo el texto `To add this component to your project, run:`.
5. Presionar `CTRL+SHIF+P` en el VCS.
6. Seleccionar `ESP-IDF: Open ESP-IDF Terminal`.
7. Pegar el comando.
8. Presionar `ENTER`.
9. Ejecutar `ESP-IDF: Full Clean`.
10. Ejecutar `ESP-IDF: Build Project`.

## Component espressif/button del framework ESP-IoT-Solution.

[button](https://docs.espressif.com/projects/esp-iot-solution/en/latest/input_device/button.html)

1. Agregarlo.
2. Dentro de la carpeta `main`, abrir el archivo `idf_component.yml`.

```
## IDF Component Manager Manifest File
dependencies:
  espressif/button: "^3.2.0"
  ## Required IDF version
  idf:
    version: ">=4.1.0"
  # # Put list of dependencies here
  # # For components maintained by Espressif:
  # component: "~1.0.0"
  # # For 3rd party components:
  # username/component: ">=1.0.0,<2.0.0"
  # username2/component2:
  #   version: "~1.0.0"
  #   # For transient dependencies `public` flag can be set.
  #   # `public` flag doesn't have an effect dependencies of the `main` component.
  #   # All dependencies of `main` are public by default.
  #   public: true

```

**NOTA: Dentro de este archivo se puede ver todos los components del framework ESP-IoT-Solution que estan agregados a la aplicacion.**
