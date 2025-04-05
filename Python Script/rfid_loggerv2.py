import serial
import csv
import time
import requests

# ---- CONFIG ----
SERIAL_PORT = 'COM5'  # Change to your Arduino's COM port
BAUD_RATE = 9600
CSV_FILE = 'rfid_log.csv'
BALANCE_FILE = 'balance_data.csv'  # Stores user balances
RECHARGE_FILE = 'recharge_data.csv'  # Stores recharge transactions
GOOGLE_SCRIPT_URL = 'Do use your own LINK'  # Replace with your script URL

# ---- SERIAL SETUP ----
ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
time.sleep(2)  # Wait for serial connection

# ---- Load balance data ----
def load_balances():
    balances = {}
    try:
        with open(BALANCE_FILE, mode='r') as file:
            reader = csv.reader(file)
            for row in reader:
                if len(row) == 2:
                    balances[row[0]] = int(row[1])  # {RFID_TAG: BALANCE}
    except FileNotFoundError:
        pass
    return balances

# ---- Save updated balance data ----
def save_balances(balances):
    with open(BALANCE_FILE, mode='w', newline='') as file:
        writer = csv.writer(file)
        for tag, balance in balances.items():
            writer.writerow([tag, balance])

# ---- Apply balance recharges ----
def apply_recharges(balances):
    try:
        with open(RECHARGE_FILE, mode='r') as file:
            reader = csv.reader(file)
            for row in reader:
                if len(row) == 2:
                    rfid_tag, recharge_amount = row[0], int(row[1])
                    if rfid_tag in balances:
                        balances[rfid_tag] += recharge_amount
                        print(f"💰 {rfid_tag} recharged with {recharge_amount}. New balance: {balances[rfid_tag]}")
        
        # Clear the recharge file after applying updates
        open(RECHARGE_FILE, 'w').close()
    except FileNotFoundError:
        pass

# ---- Log RFID scan to CSV ----
def log_to_csv(data):
    with open(CSV_FILE, mode='a', newline='') as file:
        writer = csv.writer(file)
        writer.writerow(data)

# ---- Send Data to Google Sheets ----
def send_to_google_script(rfid, balance, status, access):
    url = f"{GOOGLE_SCRIPT_URL}?tag={rfid}&balance={balance}&status={status}&access={access}"
    response = requests.get(url)
    
    if response.status_code == 200:
        print(f"✅ Data sent to Google Sheets: {response.text}")
    else:
        print(f"⚠ Google Script Error: {response.status_code}")

# ---- Main Function ----
def main():
    balances = load_balances()
    print("📡 Waiting for RFID scans...")

    while True:
        apply_recharges(balances)  # Apply recharges before processing new scans
        
        if ser.in_waiting > 0:
            rfid_tag = ser.readline().decode('utf-8').strip()
            print(f"🔹 RFID Detected: {rfid_tag}")

            if rfid_tag in balances and balances[rfid_tag] > 0:
                balances[rfid_tag] -= 10  # Deduct balance (change as needed)
                status = "Paid"
                access = "Granted"
            else:
                status = "Insufficient Balance"
                access = "Denied"

            # Log transaction locally
            log_to_csv([time.strftime("%Y-%m-%d %H:%M:%S"), rfid_tag, balances.get(rfid_tag, "N/A"), status, access])
            print("💾 Data logged locally!")

            # Update balance file
            save_balances(balances)

            # Send data to Google Sheets
            send_to_google_script(rfid_tag, balances.get(rfid_tag, "N/A"), status, access)

            time.sleep(2)  # Avoid duplicate scans

if __name__ == "__main__":
    main()
