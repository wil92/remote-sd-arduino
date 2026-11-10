def getMetaData():
    return {}


def register(app):
    from .output_device import RemoteSDPlugin
    return {"output_device": RemoteSDPlugin()}
