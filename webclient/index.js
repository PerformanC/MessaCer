import http from 'http'
import net from 'net'
import fs from 'fs'
import { WebSocketServer } from 'ws'

const wss = new WebSocketServer({ noServer: true })

wss.on('connection', (ws) => {
   ws.on('message', (data) => {
     const json = JSON.parse(data)

     if (json.op == 'connect') {
        const url = new URL(json.url)
    
        const client = net.createConnection({ port: 8888, host: url.host  }, () => {

          ws.send(JSON.stringify({
            op: 'connectionSuccess'
          }))

          client.write(JSON.stringify({
            op: 'auth',
            username: json.username,
            password: json.password
          }))
        })

        client.on('error', (err) => ws.send(JSON.stringify({
          op: 'connectionFail',
          reason: err.message
        })))

        client.on('data', (data) => ws.send(data.toString()))

        ws.on('message', (data) => client.write(data.toString()))
     }
   })
})

function decidePage(req) {
  if (req.url == '/' && req.method == 'GET') return 'login'
  if (req.url == '/submit' && req.method == 'POST') return 'submit'
  if (req.url.startsWith('/chat') && req.method == 'GET') return 'chat'
  if (req.url.startsWith('/assets') && req.method == 'GET') return 'assets'
  if (req.url == '/websocket' && req.method == 'GET') return 'websocket'
  return '404'
}

const server = http.createServer((req, res) => {
  const page = decidePage(req)
  switch (page) {
    case 'login': {    
      res.writeHead(200, { 'Content-Type': 'text/html' })
      res.write(fs.readFileSync('./login.html'))
      res.end()

      break
    }
    case 'submit': {
      let body = ''
      req.on('data', (chunk) => body += chunk)
        
      req.on('end', () => {
        const formData = new URLSearchParams(body)
        const username = formData.get('username')
        const password = formData.get('password')
        const url = formData.get('url')

        res.writeHead(302, { 'Location': `/chat?url=${url}&username=${username}&password=${password}` })
        res.end()
      })

      break
    }
    case 'chat': {
      const params = new URL(req.url, `http://${req.headers.host}`)

      if (!params.searchParams.get('url') || !params.searchParams.get('username') || !params.searchParams.get('password')) {
        res.writeHead(200, { 'Content-Type': 'text/html' })
        res.write('Bad request')
        res.end()

        return;
      }

      res.writeHead(200, { 'Content-Type': 'text/html' })
      res.write(fs.readFileSync('./chat.html'))
      res.end()

      break
    }
    case 'assets': {
      if (req.url == '/assets/login.css') {
        res.writeHead(200, { 'Content-Type': 'text/css' })
        res.write(fs.readFileSync('./assets/login.css'))
      }

      if (req.url == '/assets/chat.css') {
        res.writeHead(200, { 'Content-Type': 'text/css' })
        res.write(fs.readFileSync('./assets/chat.css'))
      }

      if (req.url == '/assets/chat.js') {
        res.writeHead(200, { 'Content-Type': 'text/javascript' })
        res.write(fs.readFileSync('./assets/chat.js'))
      }
      
      if (req.url == '/assets/login.js') {
        res.writeHead(200, { 'Content-Type': 'text/javascript' })
        res.write(fs.readFileSync('./assets/login.js'))
      }
      res.end()

      break
    }
  }
})
   
server.on('upgrade', (request, socket, head) => {
  wss.handleUpgrade(request, socket, head, (ws) => {
    wss.emit('connection', ws, request)
  })
})

server.listen(8080, () => {
  console.log('[http]: Running on port 8080.')
})
