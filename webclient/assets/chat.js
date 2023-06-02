const socket = new WebSocket(`ws${window.location.protocol == 'https:' ? 's' : ''}://${window.location.host}`)

let block = true

socket.addEventListener('open', () => {
  const urlParams = new URLSearchParams(window.location.search)

  const url = urlParams.get('url')
  const username = urlParams.get('username')
  const password = urlParams.get('password')
      socket.send(`{"op":"connect","username":"${username}","password":"${password}","url":"${url}"}`)
})

function createMessage(author, message) {
  const newMessage = document.querySelector('.container-custom').firstElementChild.cloneNode(true);
  newMessage.firstElementChild.innerText = author
  newMessage.lastElementChild.innerText = message
  document.querySelector('.container-custom').appendChild(newMessage)
}

socket.addEventListener('message', (event) => { 
  const json = JSON.parse(event.data)

  switch (json.op) {
    case 'msg': {
      createMessage(json.author, json.msg)

      break
    }

    case 'userJoin': {
      createMessage('SYSTEM', `${json.username} joined the chat.`)

      break
    }

    case 'userLeave': {
      createMessage('SYSTEM', `${json.username} left the chat.`)

      break
    }

    case 'connectionFail': {
      block = true

      createMessage('SYSTEM', `Failed to connect to server: ${json.reason}. Redirecting back to login.`)

      setTimeout(() => window.location.href = '/', 5000)

      break
    }

    case 'connectionSuccess': {
      createMessage('SYSTEM', 'Succesfully connected to server, sending auth..')

      break
    }

    case 'auth': {
      if (json.status == 'ok') {
        createMessage('SYSTEM', 'Successfully authenticated, ready to chat.')

        block = false
      } else {
        createMessage('SYSTEM', 'Authentication failed, incorrect password. Redirecting back to login.')

        setTimeout(() => window.location.href = '/', 5000)
      }

      break
    }
  }
})

socket.addEventListener('error', (event) => { 
  console.log(event)
})

const inputElement = document.getElementById('messageInput')

function sendMessage() {
  const messageText = inputElement.value

  if (messageText.length == 0 || block) return;

  createMessage(new URLSearchParams(window.location.search).get('username'), messageText)

  socket.send(`{"op":"msg","msg":"${messageText}"}`)
  
  inputElement.value = ""
}

inputElement.addEventListener('keypress', (event) => {
  if (event.key == 'Enter') {
    event.preventDefault()

    sendMessage()
  }
})