import tensorflow as tf
import numpy as np

input = tf.keras.Input(shape=(2,))
output = tf.keras.layers.Dense(3, activation=tf.nn.relu)(input)
output = tf.keras.layers.Dense(1, activation=tf.nn.sigmoid)(output)
model = tf.keras.Model(inputs=input, outputs=output)

model.compile()
model.save('model', save_format='tf')
