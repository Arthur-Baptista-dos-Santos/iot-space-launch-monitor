import random
import urllib.request
import json

estados = ['LIVRE', 'ATENCAO', 'PERIGO', 'CRITICO']
motivos = {
    'LIVRE': ['NORMAL'],
    'ATENCAO': ['TEMPERATURA ALTA', 'OBJETO SE APROXIMANDO'],
    'PERIGO': ['VAZAMENTO DE GAS DETECTADO', 'INTRUSO NA ZONA DE EXCLUSAO'],
    'CRITICO': ['GAS + TEMPERATURA CRITICA']
}

token = 'eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6InZoZXV2ZXFmc2lyYW9rd3VrdmlkIiwicm9sZSI6InNlcnZpY2Vfcm9sZSIsImlhdCI6MTc4MDQyODQ3NCwiZXhwIjoyMDk2MDA0NDc0fQ.2KDofSV9CHVx-jEaM8mMEREzUBFO90uD31Z2fta8QzA'
url = 'https://vheuveqfsiraokwukvid.supabase.co/rest/v1/telemetria'

for i in range(200):
    estado = random.choice(estados)
    motivo = random.choice(motivos[estado])
    temp = round(random.uniform(50, 85), 1) if estado == 'CRITICO' else round(random.uniform(25, 45), 1)
    umid = round(random.uniform(50, 80), 1)
    gas = random.randint(70, 95) if estado in ['PERIGO', 'CRITICO'] else random.randint(5, 44)
    dist = random.randint(50, 149) if estado == 'PERIGO' else random.randint(150, 500)
    gas_alarme = gas > 45
    intruso = dist < 150

    data = json.dumps({
        'tag': 'PLT-01',
        'estado': estado,
        'motivo': motivo,
        'temperatura': temp,
        'umidade': umid,
        'gas_pct': gas,
        'distancia_cm': dist,
        'gas_alarme': gas_alarme,
        'intruso': intruso
    }).encode('utf-8')

    req = urllib.request.Request(url, data=data, method='POST')
    req.add_header('apikey', token)
    req.add_header('Authorization', 'Bearer ' + token)
    req.add_header('Content-Type', 'application/json')
    req.add_header('Prefer', 'return=minimal')

    try:
        urllib.request.urlopen(req)
        print(f'[{i+1}/200] {estado} - {motivo}')
    except Exception as e:
        print(f'[{i+1}/200] ERRO: {e}')

print('Pronto!')
