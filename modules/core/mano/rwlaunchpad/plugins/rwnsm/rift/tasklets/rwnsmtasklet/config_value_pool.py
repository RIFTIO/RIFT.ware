import os
import pickle
import uuid


class ParameterValueError(Exception):
    pass


class ParameterValuePool(object):
    def __init__(self, log, name, value_iter):
        self._log = log
        self._name = name

        self._used_pool_values = []
        self._available_pool_values = list(value_iter)

        self._backing_filepath = os.path.join(
                os.environ["RIFT_ARTIFACTS"],
                "parameter_pools",
                self._name
                )

        self._read_used_pool_values()

    def _save_used_pool_values(self):
        dir_path = os.path.dirname(self._backing_filepath)
        if not os.path.exists(dir_path):
            try:
                os.makedirs(dir_path, exist_ok=True)
            except OSError as e:
                self._log.error("Could not create directory for save used pool: %s", str(e))

        try:
            with open(self._backing_filepath, "wb") as hdl:
                pickle.dump(self._used_pool_values, hdl)
        except OSError as e:
            self._log.error(
                    "Could not open the parameter value pool file: %s",
                    str(e))
        except pickle.PickleError as e:
            self._log.error(
                    "Could not pickle the used parameter value pool: %s",
                    str(e))

    def _read_used_pool_values(self):
        try:
            with open(self._backing_filepath, 'rb') as hdl:
                self._used_pool_values = pickle.load(hdl)

        except (OSError, EOFError):
            self._log.warning("Could not read from backing file: %s",
                              self._backing_filepath)
            self._used_pool_values = []

        except pickle.PickleError as e:
            self._log.warning("Could not unpickle the used parameter value pool from %s: %s",
                              self._backing_filepath, str(e))
            self._used_pool_values = []

        for value in self._used_pool_values:
            self._available_pool_values.remove(value)

    def get_next_unused_value(self):
        if len(self._available_pool_values) == 0:
            raise ParameterValueError("Not more parameter values to to allocate")

        next_value = self._available_pool_values[0]
        self._log.debug("Got next value for parameter pool %s: %s", self._name, next_value)

        return next_value

    def add_used_value(self, value):
        value = int(value)

        if len(self._available_pool_values) == 0:
            raise ParameterValueError("Not more parameter values to to allocate")

        if value in self._used_pool_values:
            raise ParameterValueError(
                    "Primitive value of {} was already used for pool name: {}".format(
                        value,
                        self._name,
                        )
                    )

        if value != self._available_pool_values[0]:
            raise ParameterValueError("Parameter value not the next in the available list: %s", value)

        self._available_pool_values.pop(0)
        self._used_pool_values.append(value)
        self._save_used_pool_values()

    def remove_used_value(self, value):
        if value not in self._used_pool_values:
            self._log.warning("Primitive value of %s was never allocated for pool name: %s",
                    value, self._name
                    )
            return

        self._used_pool_values.remove(value)
        self._available_pool_values.insert(0, value)
        self._save_used_pool_values()


if __name__ == "__main__":
    import logging
    logging.basicConfig(level=logging.DEBUG)
    logger = logging.getLogger("config_value_pool.py")
    name = str(uuid.uuid4())
    param_pool = ParameterValuePool(logger, name, range(1000, 2000))

    a = param_pool.get_next_unused_value()
    assert a == 1000

    param_pool.add_used_value(a)

    a = param_pool.get_next_unused_value()
    assert a == 1001
    param_pool.add_used_value(a)

    param_pool = ParameterValuePool(logger, name, range(1000, 2000))
    a = param_pool.get_next_unused_value()
    assert a == 1002

    try:
        param_pool.add_used_value(1004)
    except ParameterValueError:
        pass
    else:
        assert False

    a = param_pool.get_next_unused_value()
    assert a == 1002
    param_pool.add_used_value(1002)

    param_pool = ParameterValuePool(logger, name, range(1005, 2000))
