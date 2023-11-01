# -*- coding: utf-8 -*-


#from telegram.ext.updater import Updater
#from telegram.update import Update
#from telegram.ext.callbackcontext import CallbackContext
#from telegram.ext.commandhandler import CommandHandler
#from telegram.ext.messagehandler import MessageHandler
#from telegram.ext.filters import Filters

from telegram import InlineKeyboardButton, InlineKeyboardMarkup, Update
from telegram.ext import Application, CallbackQueryHandler, CommandHandler, ContextTypes

import requests
import json

with open("private_data.json", "r") as read_file:
    data = json.load(read_file)


scd30_url = data['scd30_url']

def get_scd30():
    resp = requests.get(url=scd30_url)
    data = resp.json()
    return data
   


async def start(update: Update, context: ContextTypes.DEFAULT_TYPE) -> None:
    keyboard = [
            [
                InlineKeyboardButton("Option 1", callback_data="1"),
                InlineKeyboardButton("Option 1", callback_data="1"),
            ],
                [InlineKeyboardButton("Option 1", callback_data="1")],
        ]
    reply_markup = InlineKeyboardMarkup(keyboard)
    await update.message.reply_text("Please choose:", reply_markup=reply_markup)

async def button(update: Update, context: ContextTypes.DEFAULT_TYPE) -> None:
    """Parses the CallbackQuery and updates the message text."""
    query = update.callback_query
    # CallbackQueries need to be answered, even if no notification to the user is needed
    # Some clients may have trouble otherwise. See https://core.telegram.org/bots/api#callbackquery
    await query.answer()
    await query.edit_message_text(text=f"Selected option: {query.data}")



#def start(update: Update, context: CallbackContext):
#    update.message.reply_text(
#                    "Hello sir, Welcome to the Bot.Please write\
#                    /help to see the commands available.")
#

async def scd30(update: Update, context: ContextTypes.DEFAULT_TYPE) -> None:
    data = get_scd30()
    message = f"timestamp: {data['timestamp']} \nCO2: {data['CO2']:.2f} ppm \nRH: {data['rHumidity']:.2f} % \
            \nTemp: {data['temperature']:.2f} \u00b0C"
    await update.message.reply_text(message)


def main() -> None:
    application = Application.builder().token(data['telegram_token']).build()
    application.add_handler(CommandHandler("start", start))
    application.add_handler(CallbackQueryHandler(button))
    application.add_handler(CommandHandler("scd30", scd30))
    application.run_polling()


if __name__ == "__main__":
    main()

#updater = Updater(,use_context=True)
#updater.dispatcher.add_handler(CommandHandler('start', start))
#updater.dispatcher.add_handler(CommandHandler('scd30', scd30))

#updater.start_polling()




