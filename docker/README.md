
Building the image:

```
docker build -t whisper_test .
```

Running the build in docker container:

```
docker run --name builder -v $(pwd):/build -ti --rm whisper_test bin/bash
```
