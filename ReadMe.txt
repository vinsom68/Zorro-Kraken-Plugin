

How to use the Kraken Plugin:

Add to Accounts.csv
Kraken,Kraken,111111,111111,11111,AssetsFix,AUD,1,1,Kraken

- Add to the zorro.ini (need the header in bracket)
The PrivateKey and PublicKey values need to be taken fron Kraken.
DisplayCurrency is to show the balance in the selected currency, usually XBT, but can be different from base currency used for trading
BaseTradeCurrency is the base currency for trading. All traded pairs must have the selected BaseTradeCurrency 
on the right of the pair (ie: DASH/XBT if set to XBT)
IntervalSec is to set a cache time to avoid calling some API to many times
DemoTrading is to allow the Buy/Sell to be only validated by the broker but would not place an order. Set it to 0 for live trading
EnableLog is to log to KrakenPlugin.log file
UseCryptoCompareHistory is to download history from CryptoCompare instead of kraken
MinOrderSize is to set the minumum order size allowed by kraken. Thereis no API to get this values 
(see https://support.kraken.com/hc/en-us/articles/205893708-What-is-the-minimum-order-size-)
You will note that Zorro 1 lot will buy/sell a MinOrderSize amount and multiples of this

[KRAKEN]
PrivateKey = "?????"
PublicKey = "??????"
DisplayCurrency="USD"
BaseTradeCurrency="XBT" 
IntervalSec = "30"
DemoTrading="1"
EnableLog="1"; 
UseCryptoCompareHistory="1"
MinOrderSize={"error":[],"result":{"REP":0.3,"XBT":0.002,"DASH":0.03,"XDG":3000,"EOS":3,"ETH":0.02,"ETC":0.3,"GNO":0.03,"ICN":2,"LTC":0.1,"MLN":0.1,"XMR":0.1,"XRP":30,"XLM":300,"ZEC":0.03,"BCH":0.002}}

- Add to AssetFix or create your own asset list file
Name,Price,Spread,RollLong,RollShort,PIP,PIPCost,MarginCost,Leverage,LotAmount,Commission,Symbol
BCH/BTC,50,0,0,0,0.00000001,0.01,0,1,1,0,BCH/XBT
DASH/BTC,50,0,0,0,0.00000001,0.01,0,1,1,0,DASH/XBT
EOS/BTC,50,0,0,0,0.00000001,0.01,0,1,1,0,EOS/XBT
ETC/BTC,50,0,0,0,0.00000001,0.01,0,1,1,0,ETC/XBT
GNO/BTC,50,0,0,0,0.00000001,0.01,0,1,1,0,GNO/XBT
ICN/BTC,50,0,0,0,0.00000001,0.01,0,1,1,0,ICN/XBT
LTC/BTC,50,0,0,0,0.00000001,0.01,0,1,1,0,LTC/XBT
MLN/BTC,50,0,0,0,0.00000001,0.01,0,1,1,0,MLN/XBT
REP/BTC,50,0,0,0,0.00000001,0.01,0,1,1,0,REP/XBT
XLM/BTC,50,0,0,0,0.00000001,0.01,0,1,1,0,XLM/XBT
XMR/BTC,50,0,0,0,0.00000001,0.01,0,1,1,0,XMR/XBT
XRP/BTC,50,0,0,0,0.00000001,0.01,0,1,1,0,XRP/XBT
ZEC/BTC,50,0,0,0,0.00000001,0.01,0,1,1,0,ZEC/XBT
ETH/BTC,50,0,0,0,0.00000001,0.01,0,1,1,0,ETH/XBT
DOGE/BTC,50,0,0,0,0.00000001,0.01,0,1,1,0,XDG/XBT

Knows Issues:

- Zorro Doesn't print/Show more than 5 decimals. So for some pairs you will see only 0. 
	For example if you trade DOGE/BTC, and have an account in BTC with balance of 0.0000010, zorro will show 0.
	Also in the Zorro log and Html report only 0s will be shown if values is outside of the 5 decimals.
- 1 minute History very limited. Option provided to use CryptoCompare history that is a bit longer.
- Sometimes you get Connection Timeout. The error shown in zorro is a parsing error, as the json parser can't parse the html page returned, you v=can get the html returned in the log file if enabled.
- Need the VC++ redistributable installed
- No leverage is supported by the plugin even though some pairs allow leverage
- NFA flag must be set in the accounts.csv
- As kraken doesn't have demo accounts, limited testing has been done with real money
- Zorro 1 lot will have a different min amount depending on the pair. See the values in the zorro.ini
- Zorro works with market orders that have a higher fee 0.26% than limit orders 0.16%
- Fees are 0.26% on kraken for market orders, so you need to approximate in your asset list something that matches an average amount that you trade for each pair, or write code to do the calculation.
- Log file when enabled get big quickly. Use it for debug only or clean it often
- Account balance when set to a currency different from base pair trading currency not working properly. Keep everything in XBT , even though for low balances you are going to see only 0s.
- If Account display in USD , balance, risk , profit for DOGE and XLM seems incorrect. Might be due to the number of decimals.


Planned to do:
- Tool to download daily history from cryptocompare
- Bittrex plugin

Suggestions for zorro team:
Allow the number of displayed and logged decimals to be configurable in zorro.ini

- Disclaimer
Use this tool at your own risk. This is a free tool developed in my free time. 


