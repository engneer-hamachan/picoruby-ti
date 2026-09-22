FROM ruby:4.0.6

RUN apt-get update \
  && apt-get install -y --no-install-recommends python3 \
  && rm -rf /var/lib/apt/lists/*

ENV EMSDK=/emsdk

RUN git clone https://github.com/emscripten-core/emsdk.git ${EMSDK} \
  && cd ${EMSDK} \
  && ./emsdk install 6.0.9 \
  && ./emsdk activate 6.0.9 \
  && chmod 777 ${EMSDK}/upstream/emscripten \
  && chmod -R 777 ${EMSDK}/upstream/emscripten/cache

ENV PATH="/emsdk:/emsdk/upstream/emscripten:${PATH}"

RUN gem install rbs -v 4.2.0

WORKDIR /work

CMD ["make", "-C", "wasm", "build"]
