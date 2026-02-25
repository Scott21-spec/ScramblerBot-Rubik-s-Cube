# ScramblerBot-Rubik's-Cube-For WCA Competitions
Robot capaz de resolver o desarmar cualquier cubo de rubik 3x3 con el uso servomotores, cámara e inteligencia artificial para la detección de colores, figuras o números.

Es la mejor opción para su uso en las competencias internacionales y nacionales de cubo de rubik (WCA), debido a que sólo se coloca el cubo dentro del robot sin necesidad de modificar el cubo o utilizar un cubo especial para su reconocimiento del estado actual.

Modos de uso:
  - Modo de Desordenamiento (Scrambler mode): 
En este modo, el sistema actúa como un actuador esclavo para competencias oficiales. Recibe de forma remota una secuencia de desordenamiento (algoritmo de scramble) validada por la WCA. Tras la ejecución mecánica automatizada de los movimientos, el servidor web local genera un reporte digital en formato PDF. Este documento incluye el registro exacto de la secuencia aplicada, el algoritmo de resolución correspondiente y representaciones gráficas del estado final del cubo (planos 2D y renderizado 3D). Por normativas de seguridad y confidencialidad competitiva, el acceso a este reporte es de carácter restringido, alojándose únicamente en el entorno del servidor para la validación exclusiva de los jueces o administradores del torneo.

  - Modo de Autogeneración y Entrenamiento (Auto-Scramble & Practice Mode):
Concebida como una herramienta de entrenamiento para competidores (speedcubers), esta modalidad permite al microcontrolador generar de manera autónoma una secuencia aleatoria de desordenamiento que cumple estrictamente con los estándares y métricas de la WCA. Una vez que el robot aplica mecánicamente la secuencia generada, el sistema emite un reporte digital homólogo al de la Modalidad 1. A diferencia del modo oficial, este documento es de acceso público y descargable a través de la interfaz web, permitiendo al usuario analizar el estado del cubo, la secuencia aplicada y la solución teórica propuesta para fines de estudio y práctica.

  - Resolución Optimizada (Solve Mode):
Orientada al análisis algorítmico, esta función permite evaluar el estado físico actual de un cubo ingresado por el usuario utilizando el sistema de visión artificial integrado. A partir del mapeo de las caras, el procesador calcula la ruta de resolución más corta (optimizando la cantidad de movimientos requeridos) y procede a su ejecución mecánica. Finalmente, el sistema documenta el proceso generando un reporte descargable que detalla el estado inicial detectado, la cinemática de los movimientos aplicados y la solución óptima ejecutada.



Proyecto de la materia 'Proyecto 3' del 8vo semestre de la carrera Mecatrónica - Facultad de Ingeniería - Universidad Nacional de Asunción - San Lorenzo - Paraguay.
