FROM python:3.12-slim
WORKDIR /app
COPY server /app/server
RUN pip install --no-cache-dir './server[production]' && useradd --uid 10001 --create-home catalog && mkdir /data && chown catalog /data
USER catalog
EXPOSE 8765
CMD ["python", "-m", "uvicorn", "server.app.main:application", "--factory", "--host", "0.0.0.0", "--port", "8765", "--no-proxy-headers"]
