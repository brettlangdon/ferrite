def application(environ, start_response):
    start_response("200 Ok", [])
    return["{}"]
