from plumbum import local

_tq = None

def init_tq_endpoint(tq_endpoint_path):
    global _tq
    _tq = local[tq_endpoint_path]

def get_queue():
    _check_tq_ep()
    queue = _tq['-l']()
    return queue

def get_results():
    _check_tq_ep()
    results = _tq['-r']()
    return results

def add_task(task_config):
    _check_tq_ep()
    (_tq["-a"] << task_config)()

def _check_tq_ep():
    assert _tq is not None, "call init_tq_endpoint(tq_endpoint_path) before"