"""Ink: layout en logica van de gezinskalender, zie ink_kalender.h.

Een external component zodat ESPHome de C++-code samen met de YAML van GitHub
kan halen (`includes:` vanuit een remote package werkt niet).
"""

import esphome.codegen as cg
import esphome.config_validation as cv

DEPENDENCIES = ["display", "i2c"]

CONFIG_SCHEMA = cv.Schema({})


async def to_code(config):
    cg.add_global(cg.RawStatement('#include "esphome/components/ink/ink_kalender.h"'))
